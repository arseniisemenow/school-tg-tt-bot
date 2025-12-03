# ADR-015: Documentation Standards

## Status
Accepted

## Context
The project requires comprehensive documentation for:
- Architecture decisions (ADRs)
- API documentation (bot commands)
- Database schema
- Deployment procedures
- Backup/restore procedures
- Development workflow
- Troubleshooting

Key requirements:
- ADR format and maintenance
- API documentation for bot commands
- Database schema documentation
- Deployment guide
- Backup/restore procedures
- Consistent documentation standards

## Decision
We will establish comprehensive documentation standards:

### ADR Documentation Format

#### ADR Template
Each ADR follows this structure:
1. **Title**: `ADR-XXX: <Short Descriptive Title>`
2. **Status**: Accepted, Proposed, Deprecated, Superseded
3. **Context**: The issue motivating this decision
4. **Decision**: The change we're proposing or have agreed to implement
5. **Consequences**: What becomes easier or more difficult
6. **Alternatives Considered**: Other options evaluated

#### ADR Numbering
- **Format**: Sequential numbers (001, 002, 003, ...)
- **Naming**: `XXX-<kebab-case-title>.md`
- **Example**: `001-technology-stack-selection.md`
- **Index**: Maintained in `docs/adr/README.md`

#### ADR Maintenance
- **New ADRs**: Create for significant architectural decisions
- **Updates**: Update status when decisions change
- **Superseding**: When ADR is superseded, mark as "Superseded" and link to new ADR
- **Deprecation**: Mark as "Deprecated" when decision is no longer relevant

#### ADR Location
- **Directory**: `docs/adr/`
- **Files**: One ADR per file
- **README**: `docs/adr/README.md` with index

### API Documentation (Bot Commands)

#### Command Documentation Format
For each bot command, document:
- **Command**: Exact command syntax
- **Description**: What the command does
- **Usage**: How to use it
- **Parameters**: Required and optional parameters
- **Examples**: Example usage
- **Permissions**: Who can use it
- **Errors**: Possible error messages

#### Example Command Documentation
```markdown
## /match

**Description**: Register a match between two players

**Usage**: `/match @player1 @player2 <score1> <score2>`

**Parameters**:
- `@player1`: Mention of first player
- `@player2`: Mention of second player
- `<score1>`: Score of first player (non-negative integer)
- `<score2>`: Score of second player (non-negative integer)

**Examples**:
- `/match @alice @bob 3 1`
- `/match @charlie @dave 2 0`

**Permissions**: Any user (if command author is one of the players, no approval needed)

**Errors**:
- "Player not found": Mentioned player doesn't exist
- "Invalid score": Score is negative or invalid
- "Rate limit exceeded": Too many commands
```

#### Command Documentation Location
- **File**: `docs/api/commands.md`
- **Organization**: Group by category (matches, ranking, admin, etc.)
- **Table of Contents**: Include TOC for easy navigation

### Database Schema Documentation

#### Schema Documentation Format
Document:
- **Table Name**: Full table name
- **Purpose**: What the table stores
- **Columns**: All columns with types and constraints
- **Indexes**: All indexes and their purpose
- **Foreign Keys**: Relationships to other tables
- **Constraints**: Check constraints, unique constraints
- **Example Data**: Example rows

#### Example Table Documentation
```markdown
## matches

**Purpose**: Stores match records between players

**Columns**:
- `id` (BIGSERIAL PRIMARY KEY): Unique match identifier
- `group_id` (BIGINT NOT NULL): Foreign key to groups table
- `player1_id` (BIGINT NOT NULL): Foreign key to players table
- `player2_id` (BIGINT NOT NULL): Foreign key to players table
- `player1_score` (INTEGER NOT NULL): Score of player 1
- `player2_score` (INTEGER NOT NULL): Score of player 2
- ...

**Indexes**:
- `idx_matches_group_created`: (group_id, created_at DESC) - For match history queries
- `idx_matches_idempotency`: (idempotency_key) UNIQUE - Prevent duplicates

**Foreign Keys**:
- `group_id` → `groups(id)`
- `player1_id` → `players(id)`
- `player2_id` → `players(id)`

**Constraints**:
- CHECK: `player1_id != player2_id` (no self-matches)
- CHECK: `player1_score >= 0 AND player2_score >= 0`
```

#### Schema Documentation Location
- **File**: `docs/database/schema.md`
- **ER Diagram**: Optional ER diagram (generated or hand-drawn)
- **Migration History**: Link to migration files

### Deployment Guide

#### Deployment Documentation Structure
1. **Prerequisites**: Required tools and accounts
2. **Environment Setup**: Environment variables and configuration
3. **Build Process**: How to build the application
4. **Database Setup**: Database creation and migrations
5. **Deployment Steps**: Step-by-step deployment
6. **Verification**: How to verify deployment
7. **Rollback**: Rollback procedures

#### Deployment Guide Location
- **File**: `docs/deployment/deployment-guide.md`
- **Environment-Specific**: Separate guides for staging and production if needed

### Backup/Restore Procedures

#### Backup Documentation
Document:
- **Backup Types**: Full backup, WAL archiving
- **Backup Schedule**: When backups run
- **Backup Location**: Where backups are stored
- **Backup Verification**: How to verify backups
- **Backup Retention**: How long backups are kept

#### Restore Documentation
Document:
- **Full Restore**: How to restore entire database
- **Point-in-Time Recovery**: How to restore to specific time
- **Selective Restore**: How to restore specific tables
- **Restore Testing**: How to test restore procedures

#### Backup/Restore Documentation Location
- **File**: `docs/operations/backup-restore.md`
- **Runbook Format**: Step-by-step procedures

### Development Documentation

#### Development Guide
Document:
- **Setup**: Local development setup
- **Architecture**: System architecture overview
- **Code Structure**: Code organization
- **Adding Features**: Guide for adding features
- **Testing**: Testing guidelines
- **Code Style**: Code style guide

#### Development Documentation Location
- **File**: `docs/development/development-guide.md`
- **Additional**: Separate files for specific topics if needed

### Troubleshooting Documentation

#### Troubleshooting Guide
Document:
- **Common Issues**: Common problems and solutions
- **Error Messages**: Error message explanations
- **Debugging**: How to debug issues
- **Logs**: Where to find logs
- **Monitoring**: How to monitor the system

#### Troubleshooting Documentation Location
- **File**: `docs/operations/troubleshooting.md`
- **Runbook Format**: Problem → Solution format

### Documentation Standards

#### Markdown Format
- **Format**: Markdown (`.md` files)
- **Style**: Consistent markdown style
- **Headers**: Use proper header hierarchy
- **Code Blocks**: Use syntax highlighting
- **Links**: Use relative links for internal docs

#### Code Examples
- **Language**: Specify language in code blocks
- **Completeness**: Provide complete, runnable examples
- **Comments**: Add comments to explain code
- **Output**: Show expected output when relevant

#### Diagrams
- **Format**: Mermaid diagrams (embedded in markdown) or images
- **Types**: Architecture diagrams, sequence diagrams, ER diagrams
- **Location**: In relevant documentation files

#### Documentation Maintenance
- **Keep Updated**: Update docs when code changes
- **Review**: Review docs during code review
- **Versioning**: Docs versioned with code (same repository)

### Documentation Structure

#### Directory Structure
```
docs/
  adr/
    README.md
    001-*.md
    002-*.md
    ...
  api/
    commands.md
  database/
    schema.md
  deployment/
    deployment-guide.md
  development/
    development-guide.md
  operations/
    backup-restore.md
    troubleshooting.md
  README.md
```

#### Main README
- **Location**: `docs/README.md`
- **Contents**:
  - Overview of documentation
  - Links to all documentation
  - Quick start guide
  - Documentation standards

### Documentation Review

#### Review Process
- **Code Review**: Review docs in same PR as code changes
- **Accuracy**: Ensure docs match implementation
- **Completeness**: Ensure all features documented
- **Clarity**: Ensure docs are clear and understandable

#### Documentation Updates
- **Trigger**: Update docs when:
  - Adding new features
  - Changing behavior
  - Fixing bugs (if behavior changes)
  - Deprecating features

## Consequences

### Positive
- **Comprehensive**: All aspects documented
- **Consistent**: Consistent format and style
- **Onboarding**: Helps new developers get started

### Negative
- **Maintenance**: Need to keep docs updated
- **Time**: Writing docs takes time
- **Review**: Docs need review like code
- **Maintainable**: Hard to maintain and update
- **Accessible**: Not easy to find and navigate
- 
### Neutral
- **Format**: Markdown is simple but requires discipline
- **Tools**: May need diagramming tools

## Alternatives Considered

### No Documentation Standards
- **Rejected**: Inconsistent docs are hard to use

### External Documentation (Wiki, Confluence)
- **Rejected**: Docs in repository are versioned with code

### Different Format (reStructuredText, AsciiDoc)
- **Rejected**: Markdown is simpler and more widely used

### Minimal Documentation
- **Rejected**: Comprehensive docs are essential for maintenance

