#include <../../nrnconf.h>
#include "hocdec.h"
#include "cabcode.h"
#include "nrn_ansi.h"
#include "nrndae_c.h"
#include "nrniv_mf.h"
#include "nrnoc2iv.h"
#include "nrndaspk.h"
#include "cvodeobj.h"
#include "netcvode.h"
#include "ivocvect.h"
#include "vrecitem.h"
#include "membfunc.h"
#include "nonvintblock.h"
#include "nrndigest.h"

#include <cerrno>
#include <numeric>


#include "spmatrix.h"
extern double* sp13mat;

#if 1 || NRNMPI
extern void (*nrnthread_v_transfer_)(NrnThread*);
extern void (*nrnmpi_v_transfer_)();
#endif

extern void (*nrn_multisplit_setup_)();
extern void* nrn_multisplit_triang(NrnThread*);
extern void* nrn_multisplit_reduce_solve(NrnThread*);
extern void* nrn_multisplit_bksub(NrnThread*);
extern void nrn_multisplit_nocap_v();
extern void nrn_multisplit_nocap_v_part1(NrnThread*);
extern void nrn_multisplit_nocap_v_part2(NrnThread*);
extern void nrn_multisplit_nocap_v_part3(NrnThread*);
extern void nrn_multisplit_adjust_rhs(NrnThread*);
#if NRNMPI
extern void (*nrn_multisplit_solve_)();
#endif

static Symbol* vsym;  // for absolute tolerance
/*
CVODE expects dy/dt = f(y) and solve (I - gamma*J)*x = b with approx to
J=df/dy.

The NEURON fixed step method sets up C*dv/dt = F(v)
by first calculating F(v) and storing it on the right hand side of
the matrix equation ( see src/nrnoc/treeset.cpp nrn_rhs() ).
It then sets up the left hand side of the matrix equation using
nrn_set_cj(1./dt); setup1_tree_matrix(); setup2_tree_matrix();
to form
(C/dt -  J(F))*dv = F(v)
After a nrn_solve() the answer, dv, is stored in the right hand side
vector.

However, one must be aware of the fact that the cvode state vector
y is not the vector y for the fixed step in two ways. 1) the cvode
state vector includes ALL states, including channel states.
2) the cvode state vector does NOT include the zero area nodes
(since the capacitance for those nodes are 0). Furthermore, cvode
cannot work with the extracellular mechanism (both because extracellular
capacitance is often 0 and because  more than one dv/dt is involved
in some of the current balance equations) or LinearMechanism (same reasons).
In that case the current balance equations are of the differential
algebraic form c*dv/dt = f(v) where c is non-diagonal and may have empty rows.
The variable step method for these cases is handled by daspk.

*/

// determine neq_ and vector of pointers to scatter/gather y
// as well as algebraic nodes (no_cap)

bool Cvode::init_global() {
#if NRNMPI
    if (!use_partrans_ && nrnmpi_numprocs > 1 && (nrnmpi_v_transfer_ || nrn_multisplit_solve_)) {
        assert(nrn_nthread == 1);  // we lack an NVector class for both
        // threads and mpi together
        // could be a lot better.
        use_partrans_ = true;
    } else
#endif
        if (!structure_change_) {
        return false;
    }
    if (ctd_[0].cv_memb_list_ == nullptr) {
        neq_ = 0;
        if (use_daspk_) {
            return true;
        }
        if (nrn_nonvint_block_ode_count(0, 0)) {
            return true;
        }
        return false;
    }
    return true;
}

void Cvode::init_eqn() {
    double vtol;

    CvMembList* cml;
    int i, j, zneq, zneq_v, zneq_cap_v;
    // printf("Cvode::init_eqn\n");
    if (nthsizes_) {
        delete[] nthsizes_;
        nthsizes_ = 0;
    }
    neq_ = 0;
    for (int id = 0; id < nctd_; ++id) {
        CvodeThreadData& z = ctd_[id];
        z.cmlcap_ = nullptr;
        z.cmlext_ = nullptr;
        for (cml = z.cv_memb_list_; cml; cml = cml->next) {
            if (cml->index == CAP) {
                z.cmlcap_ = cml;
            }
            if (cml->index == EXTRACELL) {
                z.cmlext_ = cml;
            }
        }
    }
    if (use_daspk_) {
        daspk_init_eqn();
        return;
    }
    for (NrnThread* _nt: for_threads(nrn_threads, nrn_nthread)) {
        // for lvardt, this body done only once and for ctd_[0]
        CvodeThreadData& z = ctd_[_nt->id];
        // how many ode's are there? First ones are non-zero capacitance
        // nodes with non-zero capacitance
        zneq_cap_v = 0;
        if (z.cmlcap_) {
            for (auto& ml: z.cmlcap_->ml) {
                zneq_cap_v += ml.nodecount;
            }
        }
        zneq = zneq_cap_v;
        z.neq_v_ = z.nonvint_offset_ = zneq;
        // now add the membrane mechanism ode's to the count
        for (cml = z.cv_memb_list_; cml; cml = cml->next) {
            if (auto const ode_count = memb_func[cml->index].ode_count; ode_count) {
                auto const count = ode_count(cml->index);
                for (auto& ml: cml->ml) {
                    zneq += ml.nodecount * count;
                }
            }
        }
        z.nonvint_extra_offset_ = zneq;
        z.pv_.resize(z.nonvint_extra_offset_);
        z.pvdot_.resize(z.nonvint_extra_offset_);
        zneq += nrn_nonvint_block_ode_count(zneq, _nt->id);
        z.nvsize_ = zneq;
        z.nvoffset_ = neq_;
        neq_ += zneq;
#if 0
printf("%d Cvode::init_eqn id=%d neq_v_=%d #nonvint=%d #nonvint_extra=%d nvsize=%d\n",
 nrnmpi_myid, _nt->id, z.neq_v_, z.nonvint_extra_offset_ - z.nonvint_offset_,
 z.nvsize_ - z.nonvint_extra_offset_, z.nvsize_);
#endif
        if (nth_) {
            break;
        }  // lvardt
    }
#if NRNMPI
    if (use_partrans_) {
        global_neq_ = nrnmpi_int_sum_reduce(neq_);
        // printf("%d global_neq_=%d neq=%d\n", nrnmpi_myid, global_neq_, neq_);
    }
#endif
    atolvec_alloc(neq_);
    for (int id = 0; id < nctd_; ++id) {
        CvodeThreadData& z = ctd_[id];
        // If lvardt, this is a cvode integrating a particular cell in a
        // particular thread, i.e., nth_. But nctd_ = 1.
        // If gvardt, this cvode is unique and nctd_ is nrn_nthread and
        // the relevant thread is nrn_threads + id;
        NrnThread* nt_;
        if (nctd_ > 1) {  // definitely gvardt
            nt_ = nrn_threads + id;
        } else if (nrn_nthread > 1) {  // definitely lvardt
            nt_ = nth_;
        } else {  // either way, certainly thread 0
            nt_ = nrn_threads;
        }
        double* atv = n_vector_data(atolnvec_, id);
        zneq_cap_v = 0;
        if (z.cmlcap_) {
            for (auto& ml: z.cmlcap_->ml) {
                zneq_cap_v += ml.nodecount;
            }
        }
        zneq = z.nvsize_;
        zneq_v = zneq_cap_v;

        for (i = 0; i < zneq; ++i) {
            atv[i] = ncv_->atol();
        }
        vtol = 1.;
        if (!vsym) {
            vsym = hoc_table_lookup("v", hoc_built_in_symlist);
        }
        if (vsym->extra) {
            double x;
            x = vsym->extra->tolerance;
            if (x != 0 && x < vtol) {
                vtol = x;
            }
        }
        for (i = 0; i < zneq_cap_v; ++i) {
            atv[i] *= vtol;
        }

        // mark all nodes to help with marking only no_cap nodes
        auto* const vec_rhs = nt_->node_rhs_storage();
        for (int i = z.rootnode_begin_index_; i < z.rootnode_end_index_; ++i) {
            vec_rhs[i] = 1.;
        }
        for (int i = z.vnode_begin_index_; i < z.vnode_end_index_; ++i) {
            vec_rhs[i] = 1.;
        }

        i = 0;
        if (zneq_cap_v) {
            for (auto& ml: z.cmlcap_->ml) {
                for (int j = 0; j < ml.nodecount; ++j) {
                    auto* const node = ml.nodelist[j];
                    z.pv_[i] = static_cast<double*>(node->v_handle());
                    z.pvdot_[i] = static_cast<double*>(node->rhs_handle());
                    *z.pvdot_[i] = 0.;  // only ones = 1 are no_cap
                    ++i;
                }
            }
        }

        int n_vnode = (z.vnode_end_index_ - z.vnode_begin_index_) +
                      (z.rootnode_end_index_ - z.rootnode_begin_index_);
        z.no_cap_indices_.resize(n_vnode - zneq_cap_v);
        // possibly a few more than needed.
        z.no_cap_child_indices_.resize(n_vnode - zneq_cap_v);
        int nocap_index = 0;
        int nocap_child_index = 0;
        for (int i = z.rootnode_begin_index_; i < z.rootnode_end_index_; ++i) {
            if (vec_rhs[i] > .5) {
                z.no_cap_indices_[nocap_index++] = i;
            }
        }
        for (int i = z.vnode_begin_index_; i < z.vnode_end_index_; ++i) {
            if (vec_rhs[i] > .5) {
                z.no_cap_indices_[nocap_index++] = i;
            }
            auto parent_i = nt_->_v_parent_index[i];
            if (vec_rhs[parent_i] > .5) {
                z.no_cap_child_indices_[nocap_child_index++] = i;
            }
        }
        z.no_cap_indices_.resize(nocap_index);
        z.no_cap_child_indices_.resize(nocap_child_index);

        // use the sentinal values in NODERHS to construct a new no cap membrane list
        new_no_cap_memb(z, nullptr);

        // map the membrane mechanism ode state and dstate pointers
        int ieq = zneq_v;
        for (cml = z.cv_memb_list_; cml; cml = cml->next) {
            Memb_func& mf = memb_func[cml->index];
            if (!mf.ode_count) {
                continue;
            }
            // rather than change ode_map pv,pvdot args back to double*
            // from data_handle<double>, use a small (single instance
            // ode count) temporary data handle vector and do the
            // static_cast here.
            std::vector<neuron::container::data_handle<double>> pv, pvdot;
            for (auto& ml: cml->ml) {
                if (int n; (n = mf.ode_count(cml->index)) > 0) {
                    // Note: if mf.hoc_mech then all cvode related
                    // callbacks are NULL (including ode_count)
                    // See src/nrniv/hocmech.cpp. That won't change but
                    // if it does, hocmech.cpp must follow all the
                    // nrn_ode_..._t prototypes to avoid segfault
                    // with Apple M1.
                    pv.resize(n);
                    pvdot.resize(n);
                    for (j = 0; j < ml.nodecount; ++j) {
                        mf.ode_map(ml.prop[j], ieq, pv.data(), pvdot.data(), atv + ieq, cml->index);
                        for (auto k = 0; k < n; ++k) {
                            z.pv_[k + ieq] = static_cast<double*>(pv[k]);
                            z.pvdot_[k + ieq] = static_cast<double*>(pvdot[k]);
                        }
                        ieq += n;
                    }
                }
            }
        }
        nrn_nonvint_block_ode_abstol(z.nvsize_, atv, id);
    }
    // validate pv_ and pvdot_ pointer elements as non null.
    for (int id = 0; id < nctd_; ++id) {
        CvodeThreadData& z = ctd_[id];
        nrn_assert(std::size_t(z.nonvint_extra_offset_) == z.pv_.size());
        for (int i = 0; i < z.nonvint_extra_offset_; ++i) {
            nrn_assert(z.pv_[i]);
            nrn_assert(z.pvdot_[i]);
        }
    }

    structure_change_ = false;
}

void Cvode::new_no_cap_memb(CvodeThreadData& z, NrnThread* /* thread */) {
    z.delete_memb_list(z.no_cap_memb_);
    z.no_cap_memb_ = nullptr;
    CvMembList* ncm{};
    for (auto* cml = z.cv_memb_list_; cml; cml = cml->next) {
        const Memb_func& mf = memb_func[cml->index];
        // only point processes with currents are possibilities
        if (!mf.is_point || !mf.current) {
            continue;
        }
        // count how many at no cap nodes
        int n{};
        for (auto& ml: cml->ml) {
            for (auto i = 0; i < ml.nodecount; ++i) {
                if (NODERHS(ml.nodelist[i]) > .5) {
                    ++n;
                }
            }
        }
        if (n == 0) {
            continue;
        }

        // keep same order
        if (!z.no_cap_memb_) {
            z.no_cap_memb_ = new CvMembList{cml->index};
            ncm = z.no_cap_memb_;
        } else {
            ncm->next = new CvMembList{cml->index};
            ncm = ncm->next;
        }
        ncm->next = nullptr;
        ncm->index = cml->index;
        // ncm is in non-contiguous mode
        ncm->ml.reserve(n);
        ncm->ml.clear();
        for (auto& ml: cml->ml) {
            for (auto i = 0; i < ml.nodecount; ++i) {
                if (NODERHS(ml.nodelist[i]) > .5) {
                    auto& newml = ncm->ml.emplace_back(cml->index /* mechanism type */);
                    newml.nodecount = 1;
                    newml.nodelist = new Node* [1] { ml.nodelist[i] };
                    assert(newml.nodelist[0] == ml.nodelist[i]);
                    newml.nodeindices = new int[1]{ml.nodeindices[i]};
                    newml.prop = new Prop* [1] { ml.prop[i] };
                    if (!mf.hoc_mech) {
                        // Danger: this is not stable w.r.t. permutation
                        newml.set_storage_offset(ml.get_storage_offset() + i);
                        newml.pdata = new Datum* [1] { ml.pdata[i] };
                    }
                    newml._thread = ml._thread;
                }
            }
        }
        assert(ncm->ml.size() == std::size_t(n));
    }
}

void Cvode::daspk_init_eqn() {
    // DASPK equation order is exactly the same order as the
    // fixed step method for current balance (including
    // extracellular nodes) and linear mechanism. Remaining ode
    // equations are same order as for Cvode. Thus, daspk differs from
    // cvode order primarily in that cap and no-cap nodes are not
    // distinguished.
    // note that only one thread is allowed for sparse right now.
    NrnThread* _nt = nrn_threads;
    CvodeThreadData& z = ctd_[0];
    double vtol;
    // printf("Cvode::daspk_init_eqn\n");
    int i, j, in, ie, k, zneq;

    // how many equations are there?
    neq_ = 0;
    CvMembList* cml;
    // start with all the equations for the fixed step method.
    if (use_sparse13 == 0 || diam_changed != 0) {
        recalc_diam();
    }
    zneq = spGetSize(_nt->_sp13mat, 0);
    z.neq_v_ = z.nonvint_offset_ = zneq;
    // now add the membrane mechanism ode's to the count
    for (cml = z.cv_memb_list_; cml; cml = cml->next) {
        if (auto ode_count = memb_func[cml->index].ode_count; ode_count) {
            zneq += std::accumulate(cml->ml.begin(),
                                    cml->ml.end(),
                                    0,
                                    [](int total, auto& ml) { return total + ml.nodecount; }) *
                    ode_count(cml->index);
        }
    }
    z.nonvint_extra_offset_ = zneq;
    zneq += nrn_nonvint_block_ode_count(zneq, _nt->id);
    z.nvsize_ = zneq;
    z.nvoffset_ = neq_;
    neq_ = z.nvsize_;
    // printf("Cvode::daspk_init_eqn: neq_v_=%d neq_=%d\n", neq_v_, neq_);
    z.pv_.resize(z.nonvint_extra_offset_);
    z.pvdot_.resize(z.nonvint_extra_offset_);
    atolvec_alloc(neq_);
    double* atv = n_vector_data(atolnvec_, 0);
    for (i = 0; i < neq_; ++i) {
        atv[i] = ncv_->atol();
    }
    vtol = 1.;
    if (!vsym) {
        vsym = hoc_table_lookup("v", hoc_built_in_symlist);
    }
    if (vsym->extra) {
        double x;
        x = vsym->extra->tolerance;
        if (x != 0 && x < vtol) {
            vtol = x;
        }
    }
    // deal with voltage and extracellular and linear circuit nodes
    // for daspk the order is the same
    assert(use_sparse13);
    if (use_sparse13) {
        for (in = 0; in < _nt->end; ++in) {
            Node* nd;
            Extnode* nde;
            nd = _nt->_v_node[in];
            nde = nd->extnode;
            i = nd->eqn_index_ - 1;  // the sparse matrix index starts at 1
            z.pv_[i] = static_cast<double*>(nd->v_handle());
            z.pvdot_[i] = static_cast<double*>(nd->rhs_handle());
            if (nde) {
                for (ie = 0; ie < nlayer; ++ie) {
                    k = i + ie + 1;
                    z.pv_[k] = static_cast<double*>(
                        neuron::container::data_handle<double>{nde->v + ie});
                    z.pvdot_[k] = static_cast<double*>(
                        neuron::container::data_handle<double>{neuron::container::do_not_search,
                                                               nde->_rhs[ie]});
                }
            }
        }
        nrndae_dkmap(z.pv_, z.pvdot_);
        for (i = 0; i < z.neq_v_; ++i) {
            atv[i] *= vtol;
        }
    }

    // map the membrane mechanism ode state and dstate pointers
    int ieq = z.neq_v_;
    for (cml = z.cv_memb_list_; cml; cml = cml->next) {
        auto const& mf = memb_func[cml->index];
        auto const ode_count = mf.ode_count;
        if (!ode_count) {
            continue;
        }
        auto const n = ode_count(cml->index);
        if (n <= 0) {
            continue;
        }
        auto const ode_map = mf.ode_map;
        // ode_map uses data_handle. Do static_cast<double*> here
        std::vector<neuron::container::data_handle<double>> pv(n), pvdot(n);
        for (auto& ml: cml->ml) {
            for (j = 0; j < ml.nodecount; ++j) {
                assert(ode_map);
                ode_map(ml.prop[j], ieq, pv.data(), pvdot.data(), atv + ieq, cml->index);
                for (auto k = 0; k < n; ++k) {
                    z.pv_[k + ieq] = static_cast<double*>(pv[k]);
                    z.pvdot_[k + ieq] = static_cast<double*>(pvdot[k]);
                }
                ieq += n;
            }
        }
    }
    structure_change_ = false;
}

double* Cvode::n_vector_data(N_Vector v, int tid) {
    if (!v) {
        return 0;
    }
    if (nctd_ > 1) {
        N_Vector subvec = ((N_Vector*) N_VGetArrayPointer(v))[tid];
        return N_VGetArrayPointer(subvec);
    }
    return N_VGetArrayPointer(v);
}

extern void nrn_extra_scatter_gather(int, int);

void Cvode::scatter_y(neuron::model_sorted_token const& sorted_token, double* y, int tid) {
    CvodeThreadData& z = CTD(tid);
    assert(std::size_t(z.nonvint_extra_offset_) == z.pv_.size());
    for (int i = 0; i < z.nonvint_extra_offset_; ++i) {
        *(z.pv_[i]) = y[i];
        // printf("%d scatter_y %d %d %g\n", nrnmpi_myid, tid, i,  y[i]);
    }
    for (CvMembList* cml = z.cv_memb_list_; cml; cml = cml->next) {
        const Memb_func& mf = memb_func[cml->index];
        if (mf.ode_synonym) {
            for (auto& ml: cml->ml) {
                mf.ode_synonym(sorted_token, nrn_threads[tid], ml, cml->index);
            }
        }
    }
    nrn_extra_scatter_gather(0, tid);
}

static Cvode* gather_cv;
static N_Vector gather_vec;
static void* gather_y_thread(NrnThread* nt) {
    Cvode* cv = gather_cv;
    cv->gather_y(cv->n_vector_data(gather_vec, nt->id), nt->id);
    return 0;
}
void Cvode::gather_y(N_Vector y) {
    if (nth_) {
        gather_y(N_VGetArrayPointer(y), nth_->id);
        return;
    }
    gather_cv = this;
    gather_vec = y;
    nrn_multithread_job(gather_y_thread);
}
void Cvode::gather_y(double* y, int tid) {
    CvodeThreadData& z = CTD(tid);
    nrn_extra_scatter_gather(1, tid);
    assert(std::size_t(z.nonvint_extra_offset_) == z.pv_.size());
    for (int i = 0; i < z.nonvint_extra_offset_; ++i) {
        y[i] = *(z.pv_[i]);
        // printf("gather_y %d %d %g\n", tid, i,  y[i]);
    }
}
void Cvode::scatter_ydot(double* ydot, int tid) {
    int i;
    CvodeThreadData& z = CTD(tid);
    for (i = 0; i < z.nonvint_extra_offset_; ++i) {
        *(z.pvdot_[i]) = ydot[i];
        // printf("scatter_ydot %d %d %g\n", tid, i, ydot[i]);
    }
}
static void* gather_ydot_thread(NrnThread* nt) {
    Cvode* cv = gather_cv;
    cv->gather_ydot(cv->n_vector_data(gather_vec, nt->id), nt->id);
    return 0;
}
void Cvode::gather_ydot(N_Vector y) {
    if (nth_) {
        gather_ydot(N_VGetArrayPointer(y), nth_->id);
        return;
    }
    gather_cv = this;
    gather_vec = y;
    nrn_multithread_job(gather_ydot_thread);
}
void Cvode::gather_ydot(double* ydot, int tid) {
    int i;
    if (ydot) {
        CvodeThreadData& z = CTD(tid);
        for (i = 0; i < z.nonvint_extra_offset_; ++i) {
            ydot[i] = *(z.pvdot_[i]);
            // printf("%d gather_ydot %d %d %g\n", nrnmpi_myid, tid, i, ydot[i]);
        }
    }
}

int Cvode::setup(N_Vector ypred, N_Vector fpred) {
    // printf("Cvode::setup\n");
    if (nth_) {
        return 0;
    }
    ++jac_calls_;
    CvodeThreadData& z = CTD(0);
    double gamsave = nrn_threads->_dt;
    nrn_threads->_dt = gam();
    nrn_nonvint_block_jacobian(z.nvsize_, n_vector_data(ypred, 0), n_vector_data(fpred, 0), 0);
    nrn_threads->_dt = gamsave;
    return 0;
}

int Cvode::solvex_thread(neuron::model_sorted_token const& sorted_token,
                         double* b,
                         double* y,
                         NrnThread* nt) {
    // printf("Cvode::solvex_thread %d t=%g t_=%g\n", nt->id, nt->t, t_);
    // printf("Cvode::solvex_thread %d %g\n", nt->id, gam());
    // printf("\tenter b\n");
    // for (int i=0; i < neq_; ++i) { printf("\t\t%d %g\n", i, b[i]);}
    int i;
    CvodeThreadData& z = CTD(nt->id);
    nt->cj = 1. / gam();
    nt->_dt = gam();
    if (z.nvsize_ == 0) {
        return 0;
    }
#if NRN_DIGEST
    if (nrn_digest_) {
        nrn_digest_dbl_array("solvex enter b", nt->id, t_, b, z.nvsize_);
        nrn_digest_dbl_array("solvex enter y", nt->id, t_, y, z.nvsize_);
    }
#endif
    lhs(sorted_token, nt);  // special version for cvode.
    scatter_ydot(b, nt->id);
    if (z.cmlcap_) {
        for (auto& ml: z.cmlcap_->ml) {
            nrn_mul_capacity(sorted_token, nt, &ml);
        }
    }
    auto* const vec_rhs = nt->node_rhs_storage();
    for (auto i: z.no_cap_indices_) {
        vec_rhs[i] = 0;
    }
    // solve it
#if NRNMPI
    if (nrn_multisplit_solve_) {
        (*nrn_multisplit_solve_)();
    } else
#endif
    {
        triang(nt);
        bksub(nt);
    }
    // for (i=0; i < v_node_count; ++i) {
    //	printf("%d rhs %d %g t=%g\n", nrnmpi_myid, i, VEC_RHS(i), t);
    //}
    if (ncv_->stiff() == 2) {
        solvemem(sorted_token, nt);
    } else {
        // bug here should multiply by gam
    }
    gather_ydot(b, nt->id);
    // printf("\texit b\n");
    // for (i=0; i < neq_; ++i) { printf("\t\t%d %g\n", i, b[i]);}
    nrn_nonvint_block_ode_solve(z.nvsize_, b, y, nt->id);
#if NRN_DIGEST
    if (nrn_digest_) {
        nrn_digest_dbl_array("solvex leave b", nt->id, t_, b, z.nvsize_);
    }
#endif
    return 0;
}

int Cvode::solvex_thread_part1(double* b, NrnThread* nt) {
    // printf("Cvode::solvex_thread %d t=%g t_=%g\n", nt->id, nt->t, t_);
    // printf("Cvode::solvex_thread %d %g\n", nt->id, gam());
    // printf("\tenter b\n");
    // for (int i=0; i < neq_; ++i) { printf("\t\t%d %g\n", i, b[i]);}
    int i;
    CvodeThreadData& z = ctd_[nt->id];
    nt->cj = 1. / gam();
    nt->_dt = gam();
    if (z.nvsize_ == 0) {
        return 0;
    }
    auto const sorted_token = nrn_ensure_model_data_are_sorted();
    lhs(sorted_token, nt);  // special version for cvode.
    scatter_ydot(b, nt->id);
    if (z.cmlcap_) {
        assert(z.cmlcap_->ml.size() == 1);
        nrn_mul_capacity(sorted_token, nt, &z.cmlcap_->ml[0]);
    }
    auto* const vec_rhs = nt->node_rhs_storage();
    for (auto i: z.no_cap_indices_) {
        vec_rhs[i] = 0.;
    }
    // solve it
    nrn_multisplit_triang(nt);
    return 0;
}
int Cvode::solvex_thread_part2(NrnThread* nt) {
    nrn_multisplit_reduce_solve(nt);
    return 0;
}
int Cvode::solvex_thread_part3(double* b, NrnThread* nt) {
    nrn_multisplit_bksub(nt);
    // for (i=0; i < v_node_count; ++i) {
    //	printf("%d rhs %d %g t=%g\n", nrnmpi_myid, i, VEC_RHS(i), t);
    //}
    if (ncv_->stiff() == 2) {
        solvemem(nrn_ensure_model_data_are_sorted(), nt);
    } else {
        // bug here should multiply by gam
    }
    gather_ydot(b, nt->id);
    // printf("\texit b\n");
    // for (i=0; i < neq_; ++i) { printf("\t\t%d %g\n", i, b[i]);}
    return 0;
}

void Cvode::solvemem(neuron::model_sorted_token const& sorted_token, NrnThread* nt) {
    // all the membrane mechanism matrices
    CvodeThreadData& z = CTD(nt->id);
    CvMembList* cml;
    for (cml = z.cv_memb_list_; cml; cml = cml->next) {  // probably can start at 6 or hh
        const Memb_func& mf = memb_func[cml->index];
        if (auto const ode_matsol = mf.ode_matsol; ode_matsol) {
            for (auto& ml: cml->ml) {
                ode_matsol(sorted_token, nt, &ml, cml->index);
                if (errno && nrn_errno_check(cml->index)) {
                    hoc_warning("errno set during ode jacobian solve", nullptr);
                }
            }
        }
    }
    long_difus_solve(sorted_token, 2, *nt);
}

void Cvode::fun_thread(neuron::model_sorted_token const& sorted_token,
                       double tt,
                       double* y,
                       double* ydot,
                       NrnThread* nt) {
    CvodeThreadData& z = CTD(nt->id);
#if NRN_DIGEST
    if (nrn_digest_) {
        nrn_digest_dbl_array("y", nt->id, tt, y, z.nvsize_);
    }
#endif
    fun_thread_transfer_part1(sorted_token, tt, y, nt);
    nrn_nonvint_block_ode_fun(z.nvsize_, y, ydot, nt->id);
    fun_thread_transfer_part2(sorted_token, ydot, nt);

#if NRN_DIGEST
    if (nrn_digest_ && ydot) {
        nrn_digest_dbl_array("ydot", nt->id, tt, ydot, z.nvsize_);
    }
#endif
}

void Cvode::fun_thread_transfer_part1(neuron::model_sorted_token const& sorted_token,
                                      double tt,
                                      double* y,
                                      NrnThread* nt) {
    CvodeThreadData& z = CTD(nt->id);
    nt->_t = tt;

    // fix this!!!
    nt->_dt = h();  // really does not belong here but dt is needed for events
    if (nt->_dt == 0.) {
        nt->_dt = 1e-8;
    }

    // printf("%p fun %d %.15g %g\n", this, neq_, _t, _dt);
    play_continuous_thread(tt, nt);
    if (z.nvsize_ == 0) {
        return;
    }
    scatter_y(sorted_token, y, nt->id);
#if NRNMPI
    if (use_partrans_) {
        nrnmpi_assert_opstep(opmode_, nt->_t);
    }
#endif
    nocap_v(sorted_token, nt);  // vm at nocap nodes consistent with adjacent vm
}

void Cvode::fun_thread_transfer_part2(neuron::model_sorted_token const& sorted_token,
                                      double* ydot,
                                      NrnThread* nt) {
    CvodeThreadData& z = CTD(nt->id);
    if (z.nvsize_ == 0) {
        return;
    }
#if 1 || NRNMPI
    if (nrnthread_v_transfer_) {
        (*nrnthread_v_transfer_)(nt);
    }
#endif
    before_after(sorted_token, z.before_breakpoint_, nt);
    rhs(sorted_token, nt);  // similar to nrn_rhs in treeset.cpp
#if NRNMPI
    if (nrn_multisplit_solve_) {  // non-zero area nodes need an adjustment
        nrn_multisplit_adjust_rhs(nt);
    }
#endif
    do_ode(sorted_token, *nt);
    // divide by cm and compute capacity current
    if (z.cmlcap_) {
        for (auto& ml: z.cmlcap_->ml) {
            nrn_div_capacity(sorted_token, nt, &ml);
        }
    }
    if (auto const vec_sav_rhs = nt->node_sav_rhs_storage(); vec_sav_rhs) {
        auto* const vec_area = nt->node_area_storage();
        for (int i = z.rootnode_begin_index_; i < z.rootnode_end_index_; ++i) {
            vec_sav_rhs[i] *= vec_area[i] * 0.01;  // 0.01 milliamp/cm2 * um2 is nanoamp
        }
        for (int i = z.vnode_begin_index_; i < z.vnode_end_index_; ++i) {
            vec_sav_rhs[i] *= vec_area[i] * 0.01;
        }
    }
    gather_ydot(ydot, nt->id);
    before_after(sorted_token, z.after_solve_, nt);
    // for (int i=0; i < z.neq_; ++i) { printf("\t%d %g %g\n", i, y[i], ydot?ydot[i]:-1e99);}
}

void Cvode::fun_thread_ms_part1(double tt, double* y, NrnThread* nt) {
    nt->_t = tt;

    // fix this!!!
    nt->_dt = h();  // really does not belong here but dt is needed for events
    if (nt->_dt == 0.) {
        nt->_dt = 1e-8;
    }

    // printf("%p fun %d %.15g %g\n", this, neq_, _t, _dt);
    play_continuous_thread(tt, nt);
    scatter_y(nrn_ensure_model_data_are_sorted(), y, nt->id);
#if NRNMPI
    if (use_partrans_) {
        nrnmpi_assert_opstep(opmode_, nt->_t);
    }
#endif
    nocap_v_part1(nt);  // vm at nocap nodes consistent with adjacent vm
}
void Cvode::fun_thread_ms_part2(NrnThread* nt) {
    nocap_v_part2(nt);  // vm at nocap nodes consistent with adjacent vm
}
void Cvode::fun_thread_ms_part34(double* ydot, NrnThread* nt) {
    fun_thread_ms_part3(nt);
    fun_thread_ms_part4(ydot, nt);
}
void Cvode::fun_thread_ms_part3(NrnThread* nt) {
    nocap_v_part3(nt);  // should be by itself in fun_thread_part2_5 if
                        // following is true and a gap is in 0 area node
}
void Cvode::fun_thread_ms_part4(double* ydot, NrnThread* nt) {
#if 1 || NRNMPI
    if (nrnthread_v_transfer_) {
        (*nrnthread_v_transfer_)(nt);
    }
#endif
    CvodeThreadData& z = ctd_[nt->id];
    if (z.nvsize_ == 0) {
        return;
    }
    auto const sorted_token = nrn_ensure_model_data_are_sorted();
    before_after(sorted_token, z.before_breakpoint_, nt);
    rhs(sorted_token, nt);  // similar to nrn_rhs in treeset.cpp
    nrn_multisplit_adjust_rhs(nt);
    do_ode(sorted_token, *nt);
    // divide by cm and compute capacity current
    assert(z.cmlcap_->ml.size() == 1);
    nrn_div_capacity(sorted_token, nt, &z.cmlcap_->ml[0]);
    gather_ydot(ydot, nt->id);
    before_after(sorted_token, z.after_solve_, nt);
    // for (int i=0; i < z.neq_; ++i) { printf("\t%d %g %g\n", i, y[i], ydot?ydot[i]:-1e99);}
}

void Cvode::before_after(neuron::model_sorted_token const& sorted_token,
                         BAMechList* baml,
                         NrnThread* nt) {
    for (auto* ba = baml; ba; ba = ba->next) {
        nrn_bamech_t f = ba->bam->f;
        for (auto* const ml: ba->ml) {
            for (int i = 0; i < ml->nodecount; ++i) {
                f(ml->nodelist[i], ml->pdata[i], ml->_thread, nt, ml, i, sorted_token);
            }
        }
    }
}

/*
v at nodes with capacitance is correct (from scatter v) however
v at no-cap nodes is out of date since the values are from the
previous call. v would merely be the weighted average of
the adjacent v's except for the possibility of membrane
currents at branch points. We thus need to calculate both i(v)
and di/dv at those zero area nodes so that we can solve the
algebraic equation (di/dv + a_j)*vmnew =  - i(vmold) + a_j*v_j.
The simplest case is no membrane current and root or leaf. In that
case vmnew = v_j. The next simplest case is no membrane current.
In that case, vm is the weighted sum (via the axial coefficients)
of v_j.
For now we handle only the general case when there are membrane currents
This was done by constructing a list of membrane mechanisms that
contribute to the membrane current at the nocap nodes.
*/

void Cvode::nocap_v(neuron::model_sorted_token const& sorted_token, NrnThread* _nt) {
    int i;
    CvodeThreadData& z = CTD(_nt->id);

    auto* const vec_rhs = _nt->node_rhs_storage();
    auto* const vec_d = _nt->node_d_storage();
    auto* const vec_v = _nt->node_voltage_storage();
    for (auto i: z.no_cap_indices_) {
        vec_rhs[i] = 0.;
        vec_d[i] = 0.;
    }

    // compute the i(vmold) and di/dv
    rhs_memb(sorted_token, z.no_cap_memb_, _nt);
    lhs_memb(sorted_token, z.no_cap_memb_, _nt);

    auto* const vec_b = _nt->node_b_storage();
    for (auto i: z.no_cap_indices_) {
        vec_rhs[i] += vec_d[i] * vec_v[i];
        if (i >= z.rootnode_end_index_) {
            auto const parent_i = _nt->_v_parent_index[i];
            vec_rhs[i] -= vec_b[i] * vec_v[parent_i];
            vec_d[i] -= vec_b[i];
        }
    }

    auto* const vec_a = _nt->node_a_storage();
    for (auto i: z.no_cap_child_indices_) {
        auto const parent_i = _nt->_v_parent_index[i];
        vec_rhs[parent_i] -= vec_a[i] * vec_v[i];
        vec_d[parent_i] -= vec_a[i];
    }

#if NRNMPI
    if (nrn_multisplit_solve_) {  // add up the multisplit equations
        nrn_multisplit_nocap_v();
    }
#endif

    for (auto i: z.no_cap_indices_) {
        vec_v[i] = vec_rhs[i] / vec_d[i];
    }
    // no_cap v's are now consistent with adjacent v's
}

void Cvode::nocap_v_part1(NrnThread* _nt) {
    int i;
    CvodeThreadData& z = ctd_[_nt->id];

    auto* const vec_d = _nt->node_d_storage();
    auto* const vec_rhs = _nt->node_rhs_storage();
    auto* const vec_v = _nt->node_voltage_storage();
    auto* const vec_b = _nt->node_b_storage();
    auto* const vec_a = _nt->node_a_storage();
    for (auto i: z.no_cap_indices_) {
        vec_d[i] = 0.;
        vec_rhs[i] = 0.;
    }

    // compute the i(vmold) and di/dv
    auto const sorted_token = nrn_ensure_model_data_are_sorted();
    rhs_memb(sorted_token, z.no_cap_memb_, _nt);
    lhs_memb(sorted_token, z.no_cap_memb_, _nt);

    for (auto i: z.no_cap_indices_) {  // parent axial current
        vec_rhs[i] += vec_d[i] * vec_v[i];
        if (i >= z.rootnode_end_index_) {
            auto const parent_i = _nt->_v_parent_index[i];
            vec_rhs[i] -= vec_b[i] * vec_v[parent_i];
            vec_d[i] -= vec_b[i];
        }
    }

    for (auto i: z.no_cap_child_indices_) {
        auto const parent_i = _nt->_v_parent_index[i];
        vec_rhs[parent_i] -= vec_a[i] * vec_v[i];
        vec_d[parent_i] -= vec_a[i];
    }

    nrn_multisplit_nocap_v_part1(_nt);
}
void Cvode::nocap_v_part2(NrnThread* _nt) {
    nrn_multisplit_nocap_v_part2(_nt);
}
void Cvode::nocap_v_part3(NrnThread* _nt) {
    int i;
    nrn_multisplit_nocap_v_part3(_nt);
    CvodeThreadData& z = ctd_[_nt->id];

    auto* const vec_d = _nt->node_d_storage();
    auto* const vec_rhs = _nt->node_rhs_storage();
    auto* const vec_v = _nt->node_voltage_storage();
    for (auto i: z.no_cap_indices_) {
        vec_v[i] = vec_rhs[i] / vec_d[i];
    }
    // no_cap v's are now consistent with adjacent v's
}

void Cvode::do_ode(neuron::model_sorted_token const& sorted_token, NrnThread& nt) {
    // all the membrane mechanism ode's
    CvodeThreadData& z = CTD(nt.id);
    for (auto* cml = z.cv_memb_list_; cml; cml = cml->next) {  // probably can start at 6 or hh
        if (auto* const ode_spec = memb_func[cml->index].ode_spec; ode_spec) {
            for (auto& ml: cml->ml) {
                ode_spec(sorted_token, &nt, &ml, cml->index);
                if (errno && nrn_errno_check(cml->index)) {
                    hoc_warning("errno set during ode evaluation", nullptr);
                }
            }
        }
    }
    long_difus_solve(sorted_token, 1, nt);
}

static Cvode* nonode_cv;
static void nonode_thread(neuron::model_sorted_token const& sorted_token, NrnThread& nt) {
    nonode_cv->do_nonode(sorted_token, &nt);
}
void Cvode::do_nonode(neuron::model_sorted_token const& sorted_token, NrnThread* _nt) {
    // all the hacked integrators, etc, in SOLVE procedure almost a verbatim copy of nonvint in
    // fadvance.cpp
    if (!_nt) {
        if (nrn_nthread > 1) {
            nonode_cv = this;
            nrn_multithread_job(sorted_token, nonode_thread);
            return;
        }
        _nt = nrn_threads;
    }
    CvodeThreadData& z = CTD(_nt->id);
    CvMembList* cml;
    for (cml = z.cv_memb_list_; cml; cml = cml->next) {
        const Memb_func& mf = memb_func[cml->index];
        if (!mf.state) {
            continue;
        }
        for (auto& ml: cml->ml) {
            if (!mf.ode_spec) {
                mf.state(sorted_token, _nt, &ml, cml->index);
            } else if (mf.singchan_) {
                mf.singchan_(_nt, &ml, cml->index);
            }
        }
    }
}

void Cvode::states(double* pd) {
    int i, id;
    for (id = 0; id < nctd_; ++id) {
        CvodeThreadData& z = ctd_[id];
        double* s = n_vector_data(y_, id);
        for (i = 0; i < z.nvsize_; ++i) {
            pd[i + z.nvoffset_] = s[i];
        }
    }
}

void Cvode::dstates(double* pd) {
    int i, id;
    for (id = 0; id < nctd_; ++id) {
        CvodeThreadData& z = ctd_[id];
        for (i = 0; i < z.nonvint_extra_offset_; ++i) {
            pd[i + z.nvoffset_] = *z.pvdot_[i];
        }
        nrn_nonvint_block_ode_fun(z.nvsize_, n_vector_data(y_, id), pd, id);
    }
}

void Cvode::error_weights(double* pd) {
    int i, id;
    for (id = 0; id < nctd_; ++id) {
        CvodeThreadData& z = ctd_[id];
        double* s = n_vector_data(ewtvec(), id);
        for (i = 0; i < z.nvsize_; ++i) {
            pd[i + z.nvoffset_] = s[i];
        }
    }
}

void Cvode::acor(double* pd) {
    int i, id;
    for (id = 0; id < nctd_; ++id) {
        CvodeThreadData& z = ctd_[id];
        double* s = n_vector_data(acorvec(), id);
        for (i = 0; i < z.nvsize_; ++i) {
            pd[i + z.nvoffset_] = s[i];
        }
    }
}

void Cvode::delete_prl() {
    int i;
    for (i = 0; i < nctd_; ++i) {
        CvodeThreadData& z = ctd_[i];
        if (z.play_) {
            delete z.play_;
        }
        z.play_ = nullptr;
        if (z.record_) {
            delete z.record_;
        }
        z.record_ = nullptr;
    }
}

void Cvode::record_add(PlayRecord* pr) {
    CvodeThreadData& z = CTD(pr->ith_);
    if (!z.record_) {
        z.record_ = new std::vector<PlayRecord*>();
        z.record_->reserve(1);
    }
    z.record_->push_back(pr);
}

void Cvode::record_continuous() {
    if (nth_) {  // lvardt
        record_continuous_thread(nth_);
    } else {
        auto const sorted_token = nrn_ensure_model_data_are_sorted();
        for (int i = 0; i < nrn_nthread; ++i) {
            NrnThread* nt = nrn_threads + i;
            CvodeThreadData& z = ctd_[i];
            if (z.before_step_) {
                before_after(sorted_token, z.before_step_, nt);
            }
            if (z.record_) {
                for (auto& item: *(z.record_)) {
                    item->continuous(t_);
                }
            }
        }
    }
}

void Cvode::record_continuous_thread(NrnThread* nt) {
    CvodeThreadData& z = CTD(nt->id);
    if (z.before_step_) {
        before_after(nrn_ensure_model_data_are_sorted(), z.before_step_, nt);
    }
    if (z.record_) {
        for (auto& item: *(z.record_)) {
            item->continuous(t_);
        }
    }
}

void Cvode::play_add(PlayRecord* pr) {
    CvodeThreadData& z = CTD(pr->ith_);
    if (!z.play_) {
        z.play_ = new std::vector<PlayRecord*>();
    }
    z.play_->push_back(pr);
}

void Cvode::play_continuous(double tt) {
    if (nth_) {  // lvardt
        play_continuous_thread(tt, nth_);
    } else {
        for (int i = 0; i < nrn_nthread; ++i) {
            CvodeThreadData& z = ctd_[i];
            if (z.play_) {
                for (auto& item: *(z.play_)) {
                    item->continuous(tt);
                }
            }
        }
    }
}
void Cvode::play_continuous_thread(double tt, NrnThread* nt) {
    CvodeThreadData& z = CTD(nt->id);
    if (z.play_) {
        for (auto& item: *(z.play_)) {
            item->continuous(tt);
        }
    }
}
