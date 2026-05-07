Kernel Base Class
=================

The user-facing :ref:`regressors` chapter documents the concrete
kernel classes (``Kern_Linear``, ``Kern_RBF``, ``Kern_Polynomial``,
``Kern_Quadratic``, ``Kern_LQ``, ``Kern_Sigmoid``).
The base class below is the API hook for adding new kernels.

.. doxygenclass:: tadah::models::Kern_Base
    :members:
