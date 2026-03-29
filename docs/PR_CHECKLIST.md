# PR Checklist

Use this checklist before opening or merging a PR.

## Design

- [ ] Design note written or updated first
- [ ] Scope and interfaces are clear
- [ ] Assumptions are recorded

## Tests

- [ ] Tests added or updated first where practical
- [ ] Relevant local tests pass under `nix develop`
- [ ] Broader applicable test suite was run before handoff

## Implementation

- [ ] Changes stay within the intended ownership scope
- [ ] Common logic remains host-testable
- [ ] Platform-specific code is kept thin

## Documentation

- [ ] Relevant docs updated
- [ ] Task tracker updated if scope changed
- [ ] Validation docs updated if behavior changed
- [ ] Development journal updated

## Review

- [ ] At least 3 peer review rounds completed
- [ ] Review findings addressed
- [ ] Follow-up risks documented

## CI

- [ ] CI passed
- [ ] Host unit tests passed
- [ ] Host integration tests passed
- [ ] Required build artifacts or logs are available

## Merge

- [ ] Branch is ready for integration
- [ ] Handoff notes are complete
- [ ] No unresolved blocking findings remain
