.. _savstate:

SaveState
---------



.. class:: SaveState

    The state includes :data:`t`, the voltage for all segments of all sections, 
    and all the STATEs defined in all the membrane and point process 
    mechanisms. With regard to model descriptions, it does not include 
    PARAMETERs, ASSIGNED variables. 
    It always includes 
    values for the ambiguous variable of ions such as 
    cai, ko, or ena. This can be an expensive object in terms of memory 
    storage. 
     
    The state also includes all the outstanding events (external and self) 
    and the weight vectors of all NetCon objects. For model descriptions 
    containing a NET_RECEIVE block, all the ASSIGNED variables are also included 
    in the state (this is because such models often use such variables to 
    store logic state and other values, such as the last event time t0, 
    needed to compute state variables at the next event.) 
     
    The outstanding event delivery times are absolute. 
    When restored, all outstanding 
    events will be cleared and the restored event times and NetCon info 
    will take their place. Note that it is not in general possible to 
    change the value of t in a network simulation since most NET_RECEIVE 
    blocks keep t0 (the last event time) as part of their state. 

    Example:

        .. code:: python

            from neuron import n, rxd
            from neuron.units import mV, ms
            n.load_file("stdrun.hoc")

            soma = n.Section("soma")
            soma.insert(n.hh)
            soma.nseg = 51
            cyt = rxd.Region(soma.wholetree(), name="cyt")
            c = rxd.Species(cyt, name="c", d=1, initial=lambda node: 1 if node.x < 0.5 else 0)
            c2 = rxd.Species(
                cyt, name="c2", d=0.6, initial=lambda node: 1 if node.x > 0.5 else 0
            )
            r = rxd.Rate(c, -c * (1 - c) * (0.3 - c))
            r2 = rxd.Reaction(c + c2 > c2, 1)

            n.finitialize(-65 * mV)
            soma(0).v = -30 * mV

            n.continuerun(5 * ms)

            # this function is here solely for the demo to show the state has changed
            # there's no need to do this in your code
            def get_state():
                return (
                    soma(0.5).v,
                    c.nodes(soma(0.5)).concentration[0],
                    c2.nodes(soma(0.5)).concentration[0],
                )

            s1 = get_state()   # our local copy, just to prove we saved
            s = n.SaveState()
            s.save()

            # NOTE: calling s.save() stores the state to the s object; it does not
            # store the state to a file; use s.fwrite(file_obj) for that and 
            # s.fread(file_obj) to read state from a file before restoring.

            n.continuerun(10 * ms)

            # store the current state (don't need to do this in general, this is just to show
            # that we're no longer here after the restore)
            s2 = get_state()

            # go back to the way things were at 5 * ms (when we called s.save())
            s.restore()

            # prove we successfully reverted
            assert get_state() == s1
            assert get_state() != s2


    .. versionchanged:: 8.1

        Prior to NEURON 8.1, :class:`SaveState` did not save 
        reaction-diffusion states.


    .. warning::
        The intention is that a save followed by 
        any number of simulation-continue,restore 
        pairs will give the same simulation result (assuming the simulation 
        is deterministic). Given the possibility that simulations can 
        be written to depend on a variety of computer states not saved in this 
        object, this is more an experimental question than an assertion. 
         
        Between a save and a restore, 
        it is important not to create or delete sections, NetCon objects, 
        or point processes. Do not 
        change the number of segments, insert or delete mechanisms, 
        or change the location of point processes. 
         
        Does work with the local variable timestep method if the stdrun system 
        is used since continuerun() uses cvode.solve(tstop) to integrate and 
        this returns with all states at tstop. However, if you advance using 
        fadvance() calls different cells will be at different t values in 
        general and SaveState will be useless. 

         

----



.. method:: SaveState.save


    Syntax:
        ``.save()``


    Description:
        t, voltage, state and event values are stored in the object. 

         

----



.. method:: SaveState.restore


    Syntax:
        ``.restore()``

        ``.restore(1)``


    Description:
        t, voltage, state  and event values are put back in the sections. 
        Between a save and a restore, 
        it is important not to create or delete sections, change 
        the number of segments, insert or delete mechanisms, 
        or change the location or number of point processes. 
        Before restoring states, the object checks for consistency 
        between its own data structure and the section structures. 
         
        If the arg is 1, then the event queue is not cleared and no saved events are 
        put back on the queue. Therefore any Vector.play and/or FInitializeHandler 
        events on the queue after finitialize() are not disturbed. 

         

----



.. method:: SaveState.fread


    Syntax:
        ``.fread(File)``

        ``.fread(File, close)``


    Description:
        Reads binary state data from a File object into the 
        SaveState object. (See File in ivochelp). This does 
        not change the state of the sections. (That is done with 
        \ ``.restore()``). This function opens the file defined 
        by the File object. On return the file is closed unless 
        the second arg exists and is 1. 
         
        Warning: file format depends on what 
        mechanisms are available in the executable and the order 
        that sections are created (and mechanisms inserted) 
        by the user. Also the order of NetCon, ArtificialCell, 
        PointProcess creation and just about everything else that 
        gets saved in the file. I.e. if you change your simulation 
        setup, old files may become incompatible. 
         
        In a parallel simulation, each host 
        :meth:`ParallelContext.id` , should 
        write an id specific file. Note that the set of files is 
        at least :meth:`ParallelContext.nhost` specific. 

         

----



.. method:: SaveState.fwrite


    Syntax:
        ``.fwrite(File)``


    Description:
        Opens the file defined by the *File* object, writes saved 
        binary state data to the beginning of the file. 
        On return the file is closed unless the second arg exists 
        and is 1. In that case, extra computer state information 
        may be written to the file, e.g. :meth:`Random.seq`.

         

----



.. method:: SaveState.writehoc


    Syntax:
        ``.writehoc(File)``


    Description:
        Writes saved state data as sequence of hoc statements that 
        can be read with \ ``xopen(...)``. Not implemented at this time. 


