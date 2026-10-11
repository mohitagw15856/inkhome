# Changelog

Inkhome versions are `<upstream-version>.<n>-inkhome`. Upstream changes are
in CrossPoint's own release notes.

## 1.6.5.1-inkhome (unreleased)

Based on CrossPoint Reader 1.6.5.

### Added
- Fork foundation: README, SCOPE, PLAN, versioning, release workflow that
  builds the X3/X4 binary and publishes a web flasher.
- Inkhome unit-test suite in `test/inkhome/`.

### Changed
- Over-the-air updates check Inkhome's releases instead of upstream's, and
  compare all four version segments.
- Upstream's multi-device release workflow only runs in the upstream repository.
