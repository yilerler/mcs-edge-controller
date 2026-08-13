# MCS Edge Controller Architecture Documentation

Modern edge systems frequently couple deterministic control logic with nondeterministic application software, making systems harder to maintain and verify. 

**MCS Edge Controller** introduces a contract-driven architecture that isolates OT execution from IT services through a fixed 24-byte Interface Control Document (ICD), enabling deterministic control while preserving software extensibility.

---

## The Semantic Contract

This repository organizes architectural knowledge around three complementary concerns to ensure that definitions, decisions, and actual code implementation remain traceable to one another:

*   **Architecture Knowledge Base (AKB)** — Answers **"What is the architecture?"** It describes the current canonical state (boundaries, responsibilities, contracts, and runtime behavior).
*   **Architecture Decision Records (ADR)** — Answers **"Why is the architecture this way?"** It preserves the decision history (rationale, alternatives considered, rejected alternatives, and consequences).
*   **Repository Evidence** — Answers **"Where is the proof?"** It provides the actual implementation files, source code, and validation asserts that support our architectural claims.

---

## Documentation Map

```
README.md (You are here)
docs/
   │
   ├── AKB ──► What is true now? (./docs/AKB/)
   │
   └── ADR ──► Why did we choose this? (./docs/ADR/)
```

### 1. Architecture Knowledge Base (AKB)
The AKB is organized by architectural boundary and responsibility rather than physical file layout.
*   [Browse the AKB](./docs/AKB/)

| Module | Scope / Responsibility |
| :--- | :--- |
| **[Architecture Glossary](./docs/AKB/01_Architecture_Glossary.md)** | Canonical definitions, levels, and core terminology |
| **[OT Defense Layer](./docs/AKB/02_OT_Defense_Layer.md)** | OT safety boundary, deterministic execution, and safety degradation |
| **[ICD Contract](./docs/AKB/03_ICD_Contract.md)** | IT/OT interface contract, memory assertions, and 12-byte ABI definition |
| **[Core Runtime](./docs/AKB/04_Core_Runtime.md)** | Core Bridge, Hexagonal FSM, semantic state coordination, and Intent Veto |
| **[Gateway](./docs/AKB/05_Gateway.md)** | Gateway boundary, Anti-Corruption Layer (ACL), and transport protocols |
| **[Service](./docs/AKB/06_Service.md)** | Stateless microservice boundary and domain business capabilities |

### 2. Architecture Decision Records (ADR)
All historical decisions, architectural trade-offs, and design justifications are recorded here.
*   [Browse the ADRs](./docs/ADR/)
*   **Authoring Schema**: All new design decisions must strictly follow the **[ADR-0000 Template](./docs/ADR/ADR-0000-Template.md)** to prevent format drift.

---

## How to Read (Onboarding Guide)

For a developer or reviewer onboarding to this repository, follow this cognitive navigation path to align mental models with the system design:

```
Start
  │
  ▼
[Architecture Glossary] (01_Architecture_Glossary.md)
  │
  ▼
[Relevant AKB Module] (e.g., 02_OT_Defense_Layer.md)
  │
  ├─► [Related ADR] (Why was this designed this way?)
  │
  └─► [Repository Evidence] (Where is the actual code?)
```

1.  Start with **[Architecture Glossary](./docs/AKB/01_Architecture_Glossary.md)** to align terminology and conceptual layers.
2.  Open the corresponding **AKB Module** based on the architectural boundary you are examining.
3.  Follow references in the AKB to **Related ADRs** to understand the engineering trade-offs.
4.  Verify claims by checking the **Repository Evidence** links pointing directly to implementation code.

---

## Architecture at a Glance

The high-level architecture is organized as a layered runtime that physically separates safety-critical control loops from modern IT services.

### Hierarchical Levels
```
Level 3 — Services (m3_access_control, m4_environment_monitor, m5_air_quality_monitor, m6_local_display)
         │
         ▼
Level 2 — Gateway / ACL (v5_it_gateway, v5_ot_gateway)
         │
         ▼
Level 1 — Core / Semantic Brain (v5_core_bridge)
         │
         ▼
Level 0.5 — ABI / ICD Contract (v5_interfaces / v5_ioctl_contract.h)
         │
         ▼
Level 0 — OT Defense Layer (ot_defense_layer / Linux Kernel Module)
```

---

## Dual Structure & Evidence Mapping

To ensure architectural compliance, we explicitly distinguish our **Documentation Taxonomy** from the **Physical Repository Structure** (v5.2.4 release).

### 1. Documentation Structure
```text
docs/
├── README.md
├── AKB/
│   ├── 01_Architecture_Glossary.md
│   ├── 02_OT_Defense_Layer.md
│   ├── 03_ICD_Contract.md
│   ├── 04_Core_Runtime.md
│   ├── 05_Gateway.md
│   └── 06_Service.md
└── ADR/
    ├── ADR-0000-Template.md
    ├── ADR-0001-lkm-ot-defense-layer.md
    ├── ADR-0002-fixed-12-byte-abi.md
    └── ...
```

### 2. Physical Repository Structure (v5.2.4)
```text
mcs-edge-controller-5.2.4/
├── README.md
├── docs/
├── ot_defense_layer/
└── it_edge_layer/
    └── ros2_ws/
        └── src/
            ├── core/
            │   ├── v5_interfaces/
            │   ├── v5_core_bridge/
            │   └── v5_bringup/
            ├── gateways/
            │   ├── v5_it_gateway/
            │   └── v5_ot_gateway/
            └── services/
                ├── m3_access_control/
                ├── m4_environment_monitor/
                ├── m5_air_quality_monitor/
                └── m6_local_display/
```

### 3. Architecture-to-Implementation Mapping
The table below represents the authoritative map between high-level architectural definitions (AKB) and concrete repository evidence:

| Architectural Component | Responsibility | Repository Evidence Path |
| :--- | :--- | :--- |
| **OT Defense Layer** | Deterministic kernel safety-loop; hardware interaction | `ot_defense_layer/` |
| **Core Runtime** | Edge Semantic Brain; system state and authorization FSM | `it_edge_layer/ros2_ws/src/core/v5_core_bridge/` |
| **ICD / Contract** | Static 12-byte ABI struct; memory assertions | `it_edge_layer/ros2_ws/src/core/v5_interfaces/` <br> `ot_defense_layer/include/v5_ioctl_contract.h` |
| **Gateway / ACL** | Protocol/transport adaptation; queue validation | `it_edge_layer/ros2_ws/src/gateways/` |
| **Services** | Nondeterministic, stateless domain applications | `it_edge_layer/ros2_ws/src/services/` |

---

## Documentation Status & Governance

This documentation is designed to evolve in locked-step with the architecture. Our core governance rule is **"Architecture Claims must be Backed by Executable Evidence."**

1.  **Strict Minimalism**: The README remains a thin onboarding landing page. Details must be written in the AKB/ADR layers rather than duplicated here.
2.  **No Structural Drift**: All code changes that alter boundaries or contracts must be preceded by a validated ADR update.
3.  **Concept Primer Segregation**: In accordance with keeping documentation lean, concept-oriented training materials (such as Edge Sovereignty, Intent Veto, or Stateless Boundary) are treated as metadata and will be decoupled into a separate Concept Primer space only as repeating onboarding friction points are identified.
