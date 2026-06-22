"""Concrete pymavlink adapters for the abstract ports in ``gcs.interfaces``.

This is the *only* package allowed to import ``pymavlink``. Everything above it
talks to interfaces, so the rest of the app stays transport-agnostic.
"""
