"""Abstract ports (the DIP boundary).

Higher layers (app/, ui/) depend only on these interfaces, never on a concrete
transport. The pymavlink adapters in ``gcs.mavlink`` are the only code that
implements them, so swapping the transport (serial ⇄ UDP ⇄ a future SITL bridge)
or mocking it in tests never ripples upward.
"""
