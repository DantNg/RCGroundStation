"""Application layer: thread-safe state, the background link worker and the
composition root that wires everything together.

This layer is Qt-free on purpose. The UI polls :class:`TelemetryStore` snapshots
and drains a notice queue on a timer, so the GUI thread never blocks on I/O and
the worker never touches widgets.
"""
