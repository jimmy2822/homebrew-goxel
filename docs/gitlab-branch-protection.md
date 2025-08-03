# GitLab Branch Protection Settings

## Overview

The GitLab repository has been configured with strict branch protection rules to ensure code quality and maintain a clean commit history. Both `main` and `develop` branches are now protected and can only be updated through Merge Requests with passing CI pipelines.

## Protected Branches

### Main Branch
- **Push Access**: No one (push_access_level: 0)
- **Merge Access**: Maintainers only (merge_access_level: 40)
- **Force Push**: Disabled
- **Direct Push**: ❌ Not allowed

### Develop Branch  
- **Push Access**: No one (push_access_level: 0)
- **Merge Access**: Maintainers only (merge_access_level: 40)
- **Force Push**: Disabled
- **Direct Push**: ❌ Not allowed

## Merge Request Requirements

The following settings are enforced for all merge requests:

1. **Pipeline Must Succeed** ✅
   - `only_allow_merge_if_pipeline_succeeds: true`
   - All CI tests must pass before merging

2. **All Discussions Must Be Resolved** ✅
   - `only_allow_merge_if_all_discussions_are_resolved: true`
   - All code review comments must be addressed

3. **Remove Source Branch After Merge** ✅
   - `remove_source_branch_after_merge: true`
   - Keeps the repository clean

## Workflow

### For Contributors

1. Create a feature branch from `develop`:
   ```bash
   git checkout develop
   git pull origin develop
   git checkout -b feature/your-feature-name
   ```

2. Make your changes and commit:
   ```bash
   git add .
   git commit -m "feat: your feature description"
   ```

3. Push to GitLab:
   ```bash
   git push origin feature/your-feature-name
   ```

4. Create a Merge Request:
   ```bash
   glab mr create --target-branch develop
   ```

5. Wait for CI to pass and code review

### For Maintainers

1. Review the Merge Request
2. Ensure all CI tests pass
3. Resolve all discussions
4. Merge using the GitLab UI

## CI Pipeline

All merge requests must pass the following tests:

- **TDD Tests**: All Test-Driven Development tests must pass
  - `example_voxel_tdd`
  - `test_daemon_jsonrpc_tdd`
  - `test_daemon_integration_tdd`

## Configuration Commands

These settings were configured using the GitLab API:

```bash
# Protect develop branch
glab api -X POST projects/6/protected_branches \
  -f name=develop \
  -f push_access_level=0 \
  -f merge_access_level=40 \
  -f allow_force_push=false

# Enable merge requirements
glab api -X PUT projects/6 \
  -f only_allow_merge_if_pipeline_succeeds=true \
  -f only_allow_merge_if_all_discussions_are_resolved=true
```

## Verification

To verify the current settings:

```bash
# Check protected branches
glab api projects/6/protected_branches

# Check merge request settings
glab api projects/6 | jq '.only_allow_merge_if_pipeline_succeeds, .only_allow_merge_if_all_discussions_are_resolved'
```

---

Last Updated: August 3, 2025