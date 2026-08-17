# MCS Edge Controller Architecture Documentation

MCS Edge Controller separates deterministic OT execution from nondeterministic IT services through a contract-driven architecture. A fixed 12-byte ICD provides a stable boundary between safety-critical control and extensible application software.

---

## The Semantic Contract

This repository organizes architectural knowledge around three complementary concerns to ensure that definitions, decisions, and implementation evidence remain traceable to one another:

- **Architecture Knowledge Base (AKB)** — Answers **"What is the architecture?"** It describes the current canonical state, including boundaries, responsibilities, contracts, and runtime behavior.
- **Architecture Decision Records (ADR)** — Answers **"Why is the architecture this way?"** It preserves the decision history, including rationale, alternatives considered, rejected alternatives, and consequences.
- **Repository Evidence** — Answers **"Where is the proof?"** It provides implementation files, source code, contracts, tests, and validation evidence supporting architectural claims.

Together:

```text
                    Architectural Claim
                           │
              ┌────────────┴────────────┐
              │                         │
              ▼                         ▼
             WHAT                      WHY
              │                         │
             AKB                       ADR
              │
              ▼
           Evidence
````

---

## Documentation Map

```text
README.md (You are here)
│
├── AKB ──► What is true now?
│
└── ADR ──► Why did we choose this?
```

### 1. Architecture Knowledge Base (AKB)

The AKB is organized by architectural boundary and responsibility rather than physical file layout.

The AKB owns the canonical description of the current architecture, including its architectural definitions and canonical visualizations.

→ [Browse the AKB](./docs/AKB/)

| Module                                                         | Scope / Responsibility                                                   |
| :------------------------------------------------------------- | :----------------------------------------------------------------------- |
| **[Architecture Glossary](./docs/AKB/01_Architecture_Glossary.md)** | Canonical definitions, architecture levels, and core terminology         |
| **[OT Defense Layer](./docs/AKB/02_OT_Defense_Layer.md)**           | OT safety boundary, deterministic execution, and safety degradation      |
| **[ICD Contract](./docs/AKB/03_ICD_Contract.md)**                   | IT/OT interface contract, memory assertions, and 12-byte ABI definition  |
| **[Core Runtime](./docs/AKB/04_Core_Runtime.md)**                   | Core Bridge, Hexagonal FSM, semantic state coordination, and Intent Veto |
| **[Gateway](./docs/AKB/05_Gateway.md)**                             | Gateway boundary, Anti-Corruption Layer (ACL), and transport protocols   |
| **[Service](./docs/AKB/06_Service.md)**                             | Stateless microservice boundary and domain business capabilities         |

### 2. Architecture Decision Records (ADR)

The ADR collection records the decisions that shaped the architecture.

An ADR captures the context, decision, rationale, alternatives, rejected alternatives, and consequences associated with an architectural decision.

→ [Browse the ADRs](./docs/ADR/)

**Authoring Schema:** All new ADRs should follow the **[ADR-0000 Template](./docs/ADR/ADR-0000-Template.md)** to maintain a consistent decision record structure.

---

## How to Read

For developers and reviewers onboarding to the repository, follow this cognitive navigation path:

```text
Start
  │
  ▼
[Architecture Glossary]
  │
  ▼
[Relevant AKB Module]
  │
  ├──► [Related ADR]
  │       Why was this designed this way?
  │
  └──► [Repository Evidence]
          Where is the implementation?
```

1. Start with the **[Architecture Glossary](./docs/AKB/01_Architecture_Glossary.md)** to align terminology and architectural levels.
2. Open the corresponding **AKB Module** for the architectural boundary you are examining.
3. Follow references to **Related ADRs** to understand the engineering rationale and trade-offs.
4. Follow **Repository Evidence** links to verify architectural claims against the implementation.

---

## Architecture at a Glance

The following is a **simplified orientation view** of the MCS Edge Controller architecture.

The **canonical architecture model and architectural visualizations are maintained by the AKB**. This view exists only to provide a quick orientation from the documentation landing page.

```text
Level 3 — Services
(m3_access_control, m4_environment_monitor,
 m5_air_quality_monitor, m6_local_display)
         │
         ▼
Level 2 — Gateway / ACL
(v5_it_gateway, v5_ot_gateway)
         │
         ▼
Level 1 — Core / Semantic Brain
(v5_core_bridge)
         │
         ▼
Level 0.5 — ABI / ICD Contract
(v5_interfaces / v5_ioctl_contract.h)
         │
         ▼
Level 0 — OT Defense Layer
(ot_defense_layer / Linux Kernel Module)
```

→ See the [Architecture Glossary](./docs/AKB/01_Architecture_Glossary.md) for the canonical architecture definition.

---

## Dual Structure & Evidence Mapping

The documentation taxonomy is intentionally distinct from the physical repository structure.

The documentation structure answers:

> **How is architectural knowledge organized?**

The repository structure answers:

> **Where is that architecture implemented?**

### 1. Documentation Structure

```text
docs/
├── AKB/
│   ├── 01_Architecture_Glossary.md
│   ├── 02_OT_Defense_Layer.md
│   ├── 03_ICD_Contract.md
│   ├── 04_Core_Runtime.md
│   ├── 05_Gateway.md
│   └── 06_Service.md
│
└── ADR/
    ├── ADR-0000-Template.md
    ├── ADR-0001-lkm-ot-defense-layer.md
    ├── ADR-0002-fixed-12-byte-abi.md
    └── ...
```

### 2. Physical Repository Structure — v5.2.4

```text
mcs-edge-controller-5.2.4/
├── README.md
│
├── docs/
│   ├── ADR/
│   └── AKB/
│
├── ot_defense_layer/
│   ├── include/
│   │   └── v5_ioctl_contract.h
│   ├── src/
│   │   └── mock_elc_core.c
│   └── Makefile
│
└── it_edge_layer/
    └── ros2_ws/
        └── src/
            ├── core/
            │   ├── v5_interfaces/
            │   ├── v5_core_bridge/
            │   └── v5_bringup/
            │
            ├── gateways/
            │   ├── v5_it_gateway/
            │   └── v5_ot_gateway/
            │
            └── services/
                ├── m3_access_control/
                ├── m4_environment_monitor/
                ├── m5_air_quality_monitor/
                └── m6_local_display/
```

### 3. Architecture-to-Implementation Mapping

The following table provides a coarse-grained map between architectural domains and their physical repository locations.

Detailed claim-level evidence remains owned by the relevant AKB module.

| Architectural Component | Responsibility                                            | Repository Evidence Path                                                                          |
| :---------------------- | :-------------------------------------------------------- | :------------------------------------------------------------------------------------------------ |
| **OT Defense Layer**    | Deterministic kernel safety loop and hardware interaction | `ot_defense_layer/`                                                                               |
| **Core Runtime**        | Edge Semantic Brain; system state and authorization FSM   | `it_edge_layer/ros2_ws/src/core/v5_core_bridge/`                                                  |
| **ICD / Contract**      | Static 12-byte ABI structure and memory assertions        | `it_edge_layer/ros2_ws/src/core/v5_interfaces/`<br>`ot_defense_layer/include/v5_ioctl_contract.h` |
| **Gateway / ACL**       | Protocol / transport adaptation and queue validation      | `it_edge_layer/ros2_ws/src/gateways/`                                                             |
| **Services**            | Nondeterministic, stateless domain applications           | `it_edge_layer/ros2_ws/src/services/`                                                             |

---

## Documentation Governance

The documentation is designed to evolve with the architecture while keeping architectural claims traceable to implementation evidence.

### 1. Documentation Minimalism

The README provides orientation, navigation, and documentation governance.

Architectural definitions and detailed technical claims remain owned by the AKB and ADR layers rather than being duplicated here.

### 2. Architectural Change Traceability

Changes that alter architectural boundaries, contracts, or externally observable architectural behavior should be accompanied by an appropriate ADR.

The AKB should then be updated to reflect the resulting current architectural state.

The intended relationship is:

```text
Architectural Change
        │
        ▼
      ADR
   Why / Trade-offs
        │
        ▼
      AKB
   Current State
        │
        ▼
Repository Evidence
   Implementation
```

### 3. Evidence-Backed Claims

Architecture claims should be supported by executable or inspectable evidence where applicable.

Evidence may include:

* implementation source code
* interface and contract definitions
* compile-time assertions
* tests
* validation results
* runtime behavior

The README provides coarse-grained repository orientation; the AKB provides claim-level traceability.

### 4. Concept Documentation

Concept-oriented material is intentionally kept separate from the canonical architecture definition.

Concepts such as **Edge Sovereignty**, **Intent Veto**, and **Stateless Boundary** remain defined and referenced through the appropriate AKB and ADR material.

A dedicated **Concept Primer** layer may be introduced later if recurring onboarding friction demonstrates a need for explanatory material beyond the canonical documentation.

Such a primer would explain concepts; it would not become the canonical owner of architectural definitions or decisions.

---

## Related Resources

For implementation-specific details, follow the repository evidence referenced by the relevant AKB module.

For architectural rationale and historical context, follow the related ADR.

The documentation should therefore be navigated as:

```text
README
  │
  ├──► AKB ────────► Current Architecture
  │                     │
  │                     └──► Repository Evidence
  │
  └──► ADR ────────► Decision History
```

---

## Documentation Status

The documentation structure is intended to evolve with the MCS architecture.

The current priorities are:

1. maintain a canonical and internally consistent architecture in the AKB;
2. preserve architectural decision history in the ADR collection;
3. maintain traceability between architectural claims and repository evidence; and
4. introduce additional explanatory documentation only when recurring engineering needs justify it.