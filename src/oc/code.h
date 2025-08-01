#pragma once
enum class HocReturnType {
    floating = 0,
    integer = 1,
    boolean = 2,

};
extern HocReturnType hoc_return_type_code;

extern void hoc_nopop(void);
extern void edit(void);

extern void hoc_eval(void);
void hoc_negate();
extern void hoc_add(void), hoc_sub(void), hoc_mul(void), hoc_div(void), hoc_cyclic(void),
    hoc_power(void);
void hoc_assign();
extern void hoc_bltin(void), hoc_varpush(void), hoc_constpush(void), hoc_print(void),
    hoc_varread(void);
extern void hoc_prexpr(void), hoc_prstr(void), hoc_assstr(void), hoc_pushzero(void);
extern void hoc_chk_sym_has_ndim(), hoc_chk_sym_has_ndim1(), hoc_chk_sym_has_ndim2();
void hoc_eq();
void hoc_lt();
extern void hoc_gt(void), hoc_ge(void), hoc_le(void), hoc_ne(void), hoc_and(void), hoc_or(void),
    hoc_not(void);
void hoc_arg();
extern void hoc_ifcode(void), hoc_forcode(void), hoc_shortfor(void), hoc_call(void),
    hoc_argassign(void);
extern void hoc_argrefasgn(void), hoc_argref(void), hoc_iterator(void), hoc_iterator_stmt(void);
extern void hoc_funcret(void), hoc_procret(void), hoc_Break(void), hoc_Continue(void),
    hoc_Stop(void);
extern void hoc_debug(void), hoc_evalpointer(void);
extern void hoc_newline(void), hoc_delete_symbol(void), hoc_stringarg(void), hoc_push_string(void);
extern void hoc_argrefarg(void);
extern void hoc_arayinstal(void);

/* OOP */
extern void hoc_objectvar(void), hoc_object_component(void), hoc_object_eval(void);
extern void hoc_object_asgn(void), hoc_objvardecl(void), hoc_cmp_otype(void), hoc_newobj(void);
extern void hoc_asgn_obj_to_str(void), hoc_known_type(void);
extern void hoc_objectarg(void), hoc_ob_pointer(void), hoc_constobject(void);
extern void hoc_push_current_object(void), hoc_newobj_arg(void);
extern void hoc_autoobject(void), hocobjret(void), hoc_newobj_ret(void);
/* END OOP */

/* NEWCABLE */
extern void connectsection(void), add_section(void), range_const(void), range_interpolate(void);
extern void clear_sectionlist(void), install_sectionlist(void);
extern void rangevareval(void), sec_access(void), mech_access(void);
extern void rangeobjeval(void), rangeobjevalmiddle(void);
extern void for_segment(void), for_segment1(void);
extern void sec_access_temp(void), sec_access_push(void), sec_access_pop(void);
extern void rangepoint(void), forall_section(void), hoc_ifsec(void);
extern void rangevarevalpointer(void);
extern void connectpointer(void), connect_point_process_pointer(void), nrn_cppp(void);
extern void ob_sec_access(void), sec_access_object(void);
extern void forall_sectionlist(void), connect_obsec_syntax(void);
extern void hoc_ifseclist(void), mech_uninsert(void);
extern void simpleconnectsection(void), range_interpolate_single(void);
extern void hoc_sec_internal_push(void);
/* END NEWCABLE*/
