.. _secbrows:

SectionBrowser
--------------



.. class:: SectionBrowser


    Syntax:
        ``sb = n.SectionBrowser()``

        ``sb = n.SectionBrowser(SectionList)``


    Description:
        Class that makes a visible list of all section names. 
        If the optional SectionList arg is present then only those 
        section names are shown in the browser. 

    Example:

        .. code-block::
            python

            from neuron import n, gui
            soma = n.Section('soma')
            dend = n.Section('dend')
            sl = n.SectionList([soma])  # only the soma will be shown
            sb = n.SectionBrowser(sl)
            
    
    .. image:: ../../images/secbrows-sectionList.png
        :align: center

    .. seealso::
        :class:`SectionList`, :ref:`stdrun_shape`

         

----



.. method:: SectionBrowser.select


    Syntax:
        ``.select(sec=section)``


    Description:
        specified section is highlighted. 

    Example:

        .. code-block::
            python

            from neuron import n, gui
            soma = n.Section('soma')
            axon = n.Section('axon')

            sb = n.SectionBrowser()
            sb.select(sec=axon)        

    .. image:: ../../images/secbrows-select.png
        :align: center
----



.. method:: SectionBrowser.select_action


    Syntax:
        ``sb.select_action(python_func)``


    Description:
        A Python function is executed when an item is selected (single click or 
        dragging) by the mouse. Before execution, the selected section 
        is pushed. (and	popped after the function completes.)
        A Python function is executed in the object context in which \ ``select_action`` 
        registered it. 


    Example:

        .. code-block::
            python

            from neuron import n, gui
            soma = n.Section('soma')
            axon = n.Section('axon')

            def select(sec):
                print(f'select: {sec} {type(sec)}')

            def accept(sec):
                print(f'accept: {sec}')

            sb = n.SectionBrowser()
            sb.select_action(select)
            sb.accept_action(accept)

    .. note::

        Python support for :meth:`select_action` was added in NEURON 7.5.
         

----



.. method:: SectionBrowser.accept_action


    Syntax:
        ``sb.accept_action(python_func)``


    Description:
        A Python function is executed when an item is accepted (double click) by 
        the mouse. Before execution, the selected section 
        is pushed. (and	popped after the function completes.) 
        A Python function is executed in the object context in which the  ``accept_action`` 
        registered it. 

             
    Example:

        .. code-block::
            python

            from neuron import n, gui
            soma = n.Section('soma')
            axon = n.Section('axon')

            def select(sec):
                print(f'select: {sec} {type(sec)}')

            def accept(sec):
                print(f'accept: {sec}')

            sb = n.SectionBrowser()
            sb.select_action(select)
            sb.accept_action(accept)

    .. note::

        Python support for :meth:`accept_action` was added in NEURON 7.5.
