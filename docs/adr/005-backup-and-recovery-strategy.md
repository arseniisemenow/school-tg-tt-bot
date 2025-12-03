# ADR-005: Backup and Recovery Strategy

## Status
Accepted

## Context
The bot manages critical data (matches, ELO ratings, player information) that must be protected against:
- Data loss (hardware failures, corruption)
- Accidental deletion
- Database corruption
- Point-in-time recovery needs
- Disaster recovery scenarios

Key requirements:
- Automated backup scheduling (in-app timer, no cron dependency)
- Local backup storage with rotation
- Remote backup upload (S3, Google Cloud Storage, etc.)
- Backup verification (test restore periodically, not applying)
- PostgreSQL WAL archiving for point-in-time recovery
- Full backup every 24 hours
- Keep 7-14 days of backups
- Auto-delete older backups
- WAL continuous upload
- Backup failure alerts

## Decision
We will implement a comprehensive backup and recovery strategy with the following components:

### Backup Scheduling
- **In-App Timer**: Use C++ timer/thread (no external cron dependency)
- **Full Backup Frequency**: Every 24 hours at 02:00 UTC (configurable)
- **WAL Archiving**: Continuous (as PostgreSQL generates WAL files)
- **Implementation**:
  - Background thread with periodic timer
  - Use `std::thread` and `std::chrono` for scheduling
  - Configurable schedule via config file
  - Skip backup if previous backup still running

### Full Backup Strategy
- **Tool**: `pg_dump` (PostgreSQL native tool)
- **Format**: Custom format (`-Fc`) for compression and flexibility
- **Command**: `pg_dump -Fc -h <host> -U <user> -d <database> -f <backup_file>`
- **Compression**: Built into custom format (efficient)
- **Backup Scope**: Entire database (schema + data)
- **Exclusions**: None (backup everything)
- **Backup Naming**: `backup_<timestamp>_<database>.dump`
  - Format: `backup_20240115_020000_school_bot.dump`
- **Backup Location**: Local directory first, then upload to remote

### Local Backup Storage
- **Storage Directory**: `/var/backups/school-tg-bot/` (configurable)
- **Retention Policy**: 
  - Keep 14 days of daily backups
  - Keep 4 weekly backups (first backup of each week)
  - Keep 12 monthly backups (first backup of each month)
- **Rotation Strategy**:
  - After 14 days: Delete daily backups older than 14 days
  - After 4 weeks: Delete weekly backups older than 4 weeks
  - After 12 months: Delete monthly backups older than 12 months
- **Cleanup**: Run cleanup after each backup (delete old backups)
- **Disk Space Check**: Before backup, check available space (need 2x database size)
- **Failure Handling**: If disk full, delete oldest backups, then retry

### Remote Backup Upload
- **Storage Provider**: AWS S3 (primary choice)
  - Alternative: Google Cloud Storage (configurable)
- **S3 Configuration**:
  - Bucket: `school-tg-bot-backups` (configurable)
  - Region: Same as application (for low latency)
  - Path: `backups/<year>/<month>/<backup_file>`
- **Upload Strategy**:
  - Upload immediately after local backup completes
  - Use multipart upload for large files (>100MB)
  - Retry on failure (exponential backoff, max 3 retries)
  - Verify upload with checksum comparison
- **Encryption**:
  - Server-side encryption (S3 SSE-S3 or SSE-KMS)
  - Optionally: Client-side encryption before upload
- **Access Control**: IAM role with minimal permissions (write-only to backup bucket)
- **Cost Optimization**:
  - Use S3 Standard for recent backups
  - Transition to S3 Glacier after 30 days (optional)
  - Lifecycle policy: Delete after retention period

### PostgreSQL WAL Archiving
- **Purpose**: Enable point-in-time recovery (PITR)
- **Configuration**: 
  - `wal_level = replica` (minimum for archiving)
  - `archive_mode = on`
  - `archive_command = '<script> %p %f'` (custom script)
- **Archive Script**:
  - Copy WAL file to local archive directory
  - Upload to S3 immediately (or queue for batch upload)
  - Verify upload success
  - Delete local copy after successful upload
- **Archive Location**: 
  - Local: `/var/backups/school-tg-bot/wal/` (temporary)
  - Remote: `s3://school-tg-bot-backups/wal/<year>/<month>/<wal_file>`
- **Retention**:
  - Keep WAL files for 14 days (matches backup retention)
  - Auto-delete older WAL files
  - Ensure WAL covers backup retention window
- **Monitoring**: Alert if WAL archiving fails (critical for PITR)

### Backup Verification
- **Verification Strategy**: Periodic restore testing (not applying to production)
- **Frequency**: Weekly (every Sunday at 03:00 UTC)
- **Process**:
  1. Create temporary test database
  2. Restore latest backup to test database
  3. Run integrity checks:
     - Check table row counts
     - Verify foreign key constraints
     - Run `pg_check` or similar
     - Verify ELO calculations match history
  4. Drop test database
  5. Log verification results
- **Verification Checks**:
  - Database restores successfully
  - All tables present with correct schema
  - Row counts match expected values
  - Foreign keys valid
  - No corruption detected
- **Failure Handling**: 
  - Alert on verification failure (critical)
  - Test previous backup if current fails
  - Document failures for investigation

### Backup Failure Handling
- **Failure Detection**:
  - Monitor backup process exit codes
  - Check backup file existence and size
  - Verify upload success
  - Monitor WAL archiving status
- **Retry Strategy**:
  - Retry failed backup after 1 hour
  - Max 3 retry attempts
  - After max retries: Alert and mark as failed
- **Alerting**:
  - Immediate alert on backup failure
  - Alert on consecutive failures (>2)
  - Alert on WAL archiving failure
  - Alert on verification failure
  - Alert on disk space issues
- **Notification Channels**:
  - Log to application logs
  - Send to monitoring system (OpenTelemetry)
  - Optional: Email/Slack notification (configurable)

### Backup Metadata
- **Metadata Storage**: JSON file alongside backup
- **Metadata Contents**:
  - Backup timestamp
  - Database version
  - Backup size
  - Checksum (MD5/SHA256)
  - Backup duration
  - Success/failure status
  - WAL position (LSN)
- **File Format**: `backup_<timestamp>_<database>.dump.meta`

### Recovery Procedures
- **Full Database Restore**:
  ```bash
  pg_restore -d <target_db> <backup_file>
  ```
- **Point-in-Time Recovery (PITR)**:
  1. Restore base backup
  2. Restore WAL files up to target time
  3. Configure `recovery.conf` (PostgreSQL 12+) or `postgresql.conf`
  4. Start PostgreSQL
- **Selective Restore**: 
  - Use `pg_restore` with table selection
  - Restore specific tables only
- **Documentation**: Maintain runbook with step-by-step recovery procedures

### Backup Monitoring
- **Metrics to Track**:
  - Backup success/failure rate
  - Backup duration
  - Backup size
  - Upload duration
  - WAL archive lag (time between WAL generation and archive)
  - Disk space usage
  - Remote storage usage
- **Dashboards**: 
  - Backup history (last 30 days)
  - Storage usage trends
  - Failure rates
- **Alerts**:
  - Backup failure
  - WAL archiving failure
  - Disk space < 20%
  - Remote upload failure
  - Verification failure

### Implementation Details
- **Backup Service**: Separate thread/component in application
- **Dependencies**: 
  - PostgreSQL client tools (`pg_dump`, `pg_restore`)
  - AWS SDK for C++ (or similar for S3)
  - File system access
- **Configuration**:
  - Backup schedule (cron-like expression)
  - Local backup directory
  - Remote storage credentials (from environment variables)
  - Retention policies
  - Alert thresholds

## Consequences

### Positive
- **Data Protection**: Comprehensive backup strategy protects against data loss
- **Point-in-Time Recovery**: WAL archiving enables recovery to any point in time
- **Automation**: In-app scheduling reduces operational overhead
- **Cost Effective**: Retention policies balance protection with storage costs
- **Verification**: Regular testing ensures backups are valid
- **Disaster Recovery**: Remote backups protect against local failures

### Negative
- **Storage Costs**: Remote storage incurs ongoing costs
- **Complexity**: Multiple components (local, remote, WAL, verification)
- **Resource Usage**: Backups consume CPU, disk I/O, and network bandwidth
- **Maintenance**: Need to monitor and maintain backup system
- **Dependencies**: Requires AWS SDK and PostgreSQL tools

### Neutral
- **Backup Window**: 24-hour backup window means up to 24 hours of potential data loss
- **Recovery Time**: Full restore takes time (depends on database size)

## Alternatives Considered

### External Cron Job
- **Approach**: Use system cron for scheduling
- **Rejected**: Requirement is in-app timer, no external dependencies

### Continuous Backup (Streaming Replication)
- **Approach**: Use PostgreSQL streaming replication
- **Rejected**: More complex, requires separate server, overkill for this use case

### No WAL Archiving
- **Rejected**: Need point-in-time recovery capability

### Shorter Retention (7 days)
- **Considered**: Reduce to 7 days
- **Decision**: 14 days provides better safety margin

### Longer Retention (30+ days)
- **Rejected**: Storage costs increase significantly, 14 days sufficient

### Manual Backups Only
- **Rejected**: Human error risk, easy to forget, not reliable

### Database-Level Backups Only (No WAL)
- **Rejected**: No point-in-time recovery, larger data loss window

### Different Storage Provider
- **Considered**: Google Cloud Storage, Azure Blob Storage
- **Decision**: S3 is most common, good SDK support, can be configured

### No Backup Verification
- **Rejected**: Need to ensure backups are actually restorable

### Client-Side Encryption Only
- **Rejected**: Server-side encryption sufficient, simpler implementation

