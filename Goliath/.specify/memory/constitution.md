## Goliath Codebase Constitution

**Ratified:** 2025-09-28 | **Version:** 1.0.0 | **Last Amended:** 2025-09-28

### Preamble
All contributors must ensure that any improvements or changes to this codebase:

- Maintain and enhance the complexity and functionality of the software at all times.
- Contain only real, buildable code—no stubs, pseudo-code, or placeholders are permitted under any circumstances.
- Follow the user's specifications and the existing codebase structure with 100% accuracy.
- Use the current workspace and the Specify system as already set up.

This constitution is binding for all future development and contributions.

## Core Principles

### I. Library-First
Every feature must start as a standalone, self-contained library. Libraries must be independently testable, fully documented, and have a clear, justified purpose. Organizational-only or unused libraries are strictly prohibited.

### II. CLI Interface
All libraries and modules must expose their functionality via a command-line interface (CLI). Text in/out protocol is required: input via stdin/args, output via stdout, errors via stderr. Both JSON and human-readable formats must be supported.

### III. Test-First (NON-NEGOTIABLE)
Test-Driven Development (TDD) is mandatory: tests must be written and user-approved before implementation. The Red-Green-Refactor cycle is strictly enforced. No code may be merged without passing tests.

### IV. Integration Testing
Integration tests are required for all new library contracts, contract changes, inter-service communication, and shared schemas. All integration points must be covered by automated tests.

### V. Observability & Versioning
Structured logging is required for all components. All code must be debuggable via text I/O. Versioning must follow MAJOR.MINOR.BUILD format. Breaking changes require a migration plan and user approval.

### VI. Simplicity & Complexity
Simplicity is valued, but justified complexity is required for core features. All complexity must be documented and reviewed. YAGNI (You Aren't Gonna Need It) principles apply except where complexity is essential for correctness or extensibility.

## Additional Constraints

- Technology stack must be consistent with the existing codebase unless a migration is approved.
- All code must be production-quality, buildable, and ready for deployment at all times.
- Security, performance, and compliance standards must be met for all contributions.

## Development Workflow

- All code must be peer-reviewed for compliance with this constitution.
- Code reviews must verify: complexity, buildability, test coverage, and adherence to specifications.
- No code may be merged with stubs, pseudo-code, or incomplete implementations.
- All features and fixes must be tracked via Specify and linked to user-approved specifications.

## Governance

This constitution supersedes all other practices. Amendments require full documentation, user approval, and a migration plan. All PRs and reviews must verify compliance with this constitution. Use the agent context and guidance files for runtime development guidance.
