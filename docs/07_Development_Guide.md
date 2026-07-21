## Testing Strategy

### Phase 1: SIL Chaos Testing (Current)
Currently, standard ROS 2 test files (flake8, pep257) serve as static linters. Dynamic behavior validation is achieved via runtime fault injection (Chaos Testing) within nodes like `m3_access_control`, aggressively testing the 12-byte ICD boundary resilience.

### Phase 2: Unit Testing & HIL (Roadmap)
Future iterations will decouple the chaos logic from business nodes into a dedicated Test Harness, enabling pure unit tests for state machine transitions.