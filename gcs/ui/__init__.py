"""PySide6 presentation layer.

Widgets are passive views: they receive a ``TelemetrySnapshot`` (or notices) and
render. They never open links or send commands directly — the MainWindow routes
user intent to the :class:`~gcs.app.controller.GcsController`.
"""
