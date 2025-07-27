# Goxel v14.6 Documentation Standards

## Purpose

This document defines the standards and guidelines for all Goxel v14.6 documentation to ensure consistency, clarity, and maintainability.

## Documentation Principles

### 1. Clarity First
- Use simple, direct language
- Avoid jargon unless necessary (define it when used)
- Write for an international audience
- One concept per paragraph

### 2. Completeness
- Cover all features and edge cases
- Include both "how" and "why"
- Provide context and background
- Link to related topics

### 3. Accuracy
- Test all code examples
- Verify technical details
- Keep version information current
- Mark deprecated features clearly

### 4. Accessibility
- Use proper heading hierarchy
- Include alt text for images
- Ensure good contrast in diagrams
- Provide text alternatives for videos

## File Structure

### Naming Conventions
```
# Good
api-reference.md          # Kebab-case for files
quick-start.md           # Descriptive names
user-guide.md            # Standard suffixes

# Bad  
APIReference.md          # Avoid PascalCase
qs.md                   # Too abbreviated
userguidefinal2.md      # No versions in names
```

### Directory Organization
```
docs/v14.6/
├── README.md                 # Overview and index
├── quick-start.md           # Getting started
├── user-guide.md            # End-user documentation
├── developer-guide.md       # Developer documentation
├── api-reference.md         # API documentation
├── deployment-guide.md      # Operations guide
├── migration-guide.md       # Upgrade guide
├── troubleshooting.md       # Problem solving
├── examples/                # Code examples
│   ├── basic/              # Simple examples
│   ├── advanced/           # Complex examples
│   └── integration/        # Integration examples
├── images/                  # Documentation images
├── templates/              # Documentation templates
└── archive/                # Old documentation
```

## Markdown Standards

### Headings
```markdown
# Page Title (H1 - one per document)

## Major Section (H2)

### Subsection (H3)

#### Detail Section (H4)

Avoid H5 and H6 - restructure if needed
```

### Code Blocks
````markdown
```language
// Always specify language for syntax highlighting
const example = "code";
```

```bash
# Shell commands show prompt when helpful
$ npm install @goxel/client
```

```json
// JSON examples should be valid
{
  "key": "value"
}
```
````

### Lists
```markdown
<!-- Unordered lists for related items -->
- First item
- Second item
  - Nested item
  - Another nested item

<!-- Ordered lists for steps -->
1. First step
2. Second step
3. Third step

<!-- Task lists for tracking -->
- [x] Completed task
- [ ] Pending task
```

### Tables
```markdown
| Column 1 | Column 2 | Column 3 |
|----------|----------|----------|
| Data 1   | Data 2   | Data 3   |
| Data 4   | Data 5   | Data 6   |

<!-- Alignment -->
| Left | Center | Right |
|:-----|:------:|------:|
| L    | C      | R     |
```

### Links and References
```markdown
<!-- Internal links (relative) -->
[User Guide](user-guide.md)
[API Reference](api-reference.md#method-name)

<!-- External links (absolute) -->
[Goxel Website](https://goxel.xyz)

<!-- Reference-style links for repeated URLs -->
[Documentation][docs]
[API Reference][api]

[docs]: https://docs.goxel.xyz
[api]: https://api.goxel.xyz
```

### Images
```markdown
<!-- Basic image -->
![Alt text](images/screenshot.png)

<!-- Image with title -->
![Alt text](images/diagram.png "Hover text")

<!-- Image with link -->
[![Alt text](images/thumbnail.png)](images/full-size.png)
```

## Content Guidelines

### API Documentation

#### Method Documentation Template
```markdown
## method_name

Brief description of what the method does.

### Syntax
```javascript
result = await client.method_name(param1, param2, options)
```

### Parameters

| Parameter | Type | Required | Description | Default |
|-----------|------|----------|-------------|---------|
| param1 | string | Yes | Description | - |
| param2 | number | No | Description | 0 |

### Return Value

Description of return value

### Examples

```javascript
// Example 1: Basic usage
const result = await client.method_name("value");
```

### Errors

| Error Code | Description |
|------------|-------------|
| -32602 | Invalid parameters |

### See Also
- [Related Method](api-reference.md#related-method)
```

### Tutorial Structure

```markdown
# Tutorial: [Title]

## Overview
What you'll learn and prerequisites

## Step 1: [First Step]
Clear instructions with code

## Step 2: [Second Step]
Build on previous step

## Complete Code
Full working example

## Next Steps
Where to go from here
```

## Writing Style

### Voice and Tone
- **Active voice**: "The daemon processes requests" not "Requests are processed by the daemon"
- **Present tense**: "The method returns" not "The method will return"
- **Direct address**: "You can configure" not "One can configure"
- **Encouraging**: "Let's explore" not "You must understand"

### Common Patterns

#### Introducing Concepts
```markdown
Goxel uses a daemon architecture to improve performance. The daemon:
- Eliminates startup overhead
- Enables connection pooling  
- Supports concurrent operations
```

#### Explaining Procedures
```markdown
To start the daemon:

1. Open a terminal
2. Run `goxel --headless --daemon`
3. Verify it's running with `goxel --headless --daemon status`
```

#### Providing Examples
```markdown
Here's a simple example that creates a red cube:

```python
# Create a 3x3x3 red cube
for x in range(3):
    for y in range(3):
        for z in range(3):
            client.add_voxel(x, y, z, [255, 0, 0, 255])
```
```

## Version Documentation

### Version References
```markdown
<!-- Specific version -->
Added in v14.6.0

<!-- Version range -->
Available in v14.6+

<!-- Deprecation notice -->
> **Deprecated in v14.7**: Use `new_method` instead

<!-- Breaking change -->
> **Breaking Change in v15.0**: Parameter order changed
```

### Changelog Format
```markdown
## [14.6.0] - 2025-01-27

### Added
- Unified binary architecture
- JSON-RPC daemon mode
- Client libraries

### Changed
- Improved performance by 700%
- Updated API methods

### Fixed
- Memory leak in export function
- Connection timeout issues

### Deprecated
- Old CLI bridge mode

### Removed
- Legacy socket implementation
```

## Documentation Types

### 1. Reference Documentation
- Complete API listings
- Parameter descriptions
- Return values
- Error codes
- Examples for each method

### 2. Conceptual Documentation
- Architecture overviews
- Design decisions
- Best practices
- Performance considerations

### 3. Procedural Documentation
- Step-by-step guides
- Tutorials
- How-to articles
- Troubleshooting guides

### 4. Quick References
- Cheat sheets
- Command references
- Keyboard shortcuts
- Common patterns

## Quality Checklist

Before publishing documentation:

- [ ] **Accuracy**: All information is correct
- [ ] **Completeness**: All features are documented
- [ ] **Clarity**: Language is clear and simple
- [ ] **Examples**: Code examples work correctly
- [ ] **Links**: All links are valid
- [ ] **Formatting**: Markdown renders correctly
- [ ] **Spelling**: No spelling errors
- [ ] **Grammar**: Proper grammar throughout
- [ ] **Consistency**: Follows these standards
- [ ] **Accessibility**: Readable by all users

## Tools and Automation

### Linting
```bash
# Markdown linting
npm install -g markdownlint-cli
markdownlint docs/**/*.md

# Link checking
npm install -g markdown-link-check
find docs -name '*.md' -exec markdown-link-check {} \;
```

### Spell Checking
```bash
# Install aspell
sudo apt-get install aspell

# Check spelling
aspell check docs/user-guide.md
```

### Preview
```bash
# Local preview server
npx @compodoc/live-server --port=8080 --root=docs/v14.6
```

## Review Process

1. **Self Review**: Author reviews against checklist
2. **Technical Review**: Developer verifies accuracy
3. **Editorial Review**: Documentation team checks style
4. **User Testing**: Target audience validates clarity
5. **Final Approval**: Lead approves for publication

## Maintenance

### Regular Updates
- Review quarterly for accuracy
- Update for each release
- Monitor user feedback
- Track documentation metrics

### Deprecation Process
1. Mark as deprecated with version
2. Provide migration path
3. Keep for 2 major versions
4. Move to archive/

---

*These standards ensure Goxel documentation remains helpful, accurate, and consistent.*