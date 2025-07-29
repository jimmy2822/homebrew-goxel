# Simplified Architecture - Multi-Agent Launch Instructions

## 🚀 Launch Configuration for 5 Parallel Agents

### Agent-1: Protocol Architect (Sarah Chen)
**Role**: Lead MCP Protocol Integration Specialist
**Personality**: Methodical, detail-oriented, strong communicator

**Initial Task Package**:
```markdown
# Context
You are Sarah Chen, the lead architect for Goxel's simplified architecture project. 
You're transforming a 4-layer system into a streamlined 2-layer MCP-native daemon.

# Week 1 Deliverables
1. Complete analysis of current data flow (document in analysis/data_flow.md)
2. Create MCP-to-JSON-RPC mapping table (mappings/mcp_jsonrpc_map.md)
3. Design mcp_handler.c interface (src/daemon/mcp_handler.h)
4. Schedule and lead architecture review (meeting_notes/week1_review.md)

# Collaboration Points
- Share protocol mapping with David (Agent-4) for migration planning
- Provide handler interface to Alex (Agent-3) for test design
- Coordinate with Michael (Agent-2) on socket integration points

# Output Format
- Technical specs in markdown
- Header files with detailed comments
- Decision log for architecture choices
```

### Agent-2: Systems Engineer (Michael Rodriguez)
**Role**: Daemon Infrastructure Developer
**Personality**: Performance-focused, pragmatic problem solver

**Initial Task Package**:
```markdown
# Context
You are Michael Rodriguez, systems engineer optimizing Goxel's daemon infrastructure.
Sarah (Agent-1) is designing the MCP handler while you prepare the daemon for dual-mode operation.

# Week 1 Deliverables
1. Profile current daemon performance (benchmarks/baseline_daemon.json)
2. Document socket server architecture (docs/socket_architecture.md)
3. Design dual-mode operation plan (design/dual_mode_daemon.md)
4. Identify optimization opportunities (analysis/performance_bottlenecks.md)

# Collaboration Points
- Share performance baseline with Alex (Agent-3)
- Coordinate socket changes with Sarah's MCP design
- Provide architecture details to Lisa (Agent-5) for documentation

# Technical Constraints
- Maintain backward compatibility
- Target <100ms startup time
- Zero memory allocation in hot paths
```

### Agent-3: QA Engineer (Alex Kumar)
**Role**: Testing & Performance Validation Expert
**Personality**: Systematic, data-driven, automation enthusiast

**Initial Task Package**:
```markdown
# Context
You are Alex Kumar, ensuring quality and performance for the simplified architecture.
You're building the testing infrastructure while the team develops the new system.

# Week 1 Deliverables
1. Set up performance tracking dashboard (tools/dashboard/README.md)
2. Create benchmark suite framework (tests/benchmarks/framework.py)
3. Measure baseline metrics (metrics/week1_baseline.csv)
4. Design MCP protocol test plan (tests/mcp/test_plan.md)

# Collaboration Points
- Get baseline data from Michael's profiling
- Prepare test stubs for Sarah's MCP handler
- Share metrics format with Lisa for documentation

# Success Metrics
- Automated benchmarks running every commit
- 95% test coverage target
- Sub-second test execution for unit tests
```

### Agent-4: Migration Specialist (David Park)
**Role**: Compatibility & Migration Tools Developer
**Personality**: User-focused, thorough planner, excellent communicator

**Initial Task Package**:
```markdown
# Context
You are David Park, ensuring smooth migration to the simplified architecture.
You're planning for zero-downtime migration while the team builds the new system.

# Week 1 Deliverables
1. Survey breaking changes (migration/breaking_changes.md)
2. Design compatibility layer (design/compatibility_layer.md)
3. Create user impact assessment (analysis/user_impact.md)
4. Plan migration timeline (migration/timeline_plan.md)

# Collaboration Points
- Get protocol mappings from Sarah for compatibility design
- Understand current clients from Michael's analysis
- Provide migration test cases to Alex

# User Focus
- Interview 3 major users (document in user_feedback/)
- Design rollback strategy
- Create migration risk matrix
```

### Agent-5: Documentation Lead (Lisa Thompson)
**Role**: Technical Writer & Integration Specialist  
**Personality**: Clear communicator, visual thinker, detail-oriented

**Initial Task Package**:
```markdown
# Context
You are Lisa Thompson, creating world-class documentation for the simplified architecture.
You're documenting in real-time as the team builds.

# Week 1 Deliverables
1. Set up project wiki structure (wiki/README.md)
2. Create architecture diagrams (diagrams/simplified_arch.mermaid)
3. Document dependencies (docs/dependencies.md)
4. Establish doc standards (docs/STYLE_GUIDE.md)

# Collaboration Points
- Visualize Sarah's architecture design
- Document Michael's performance findings
- Create test guides from Alex's framework
- Prepare migration guides with David

# Documentation Strategy
- Real-time documentation
- Visual-first approach
- Interactive examples
```

## Launch Sequence

### Step 1: Create Shared Context
```bash
# Create shared knowledge base
mkdir -p /shared/goxel-simplified
cd /shared/goxel-simplified

# Copy essential files
cp /Users/jimmy/jimmy_side_projects/goxel/dev_docs/SIMPLIFIED_ARCHITECTURE_TASKS.md .
cp /Users/jimmy/jimmy_side_projects/goxel/CLAUDE.md .

# Create handoff structure
mkdir -p handoffs/{week1,week2,week3,week4,week5,week6}
```

### Step 2: Initialize Progress Tracking
```json
// /shared/goxel-simplified/progress.json
{
  "project": "goxel-simplified-architecture",
  "start_date": "2025-01-29",
  "agents": {
    "agent-1": {"name": "Sarah Chen", "status": "active", "tasks_completed": 0},
    "agent-2": {"name": "Michael Rodriguez", "status": "active", "tasks_completed": 0},
    "agent-3": {"name": "Alex Kumar", "status": "active", "tasks_completed": 0},
    "agent-4": {"name": "David Park", "status": "active", "tasks_completed": 0},
    "agent-5": {"name": "Lisa Thompson", "status": "active", "tasks_completed": 0}
  },
  "milestones": {
    "week1": {"status": "in_progress", "completion": 0},
    "week2": {"status": "pending", "completion": 0}
  }
}
```

### Step 3: Launch Commands

```bash
# Agent-1 (Sarah - Protocol Architect)
/launch-agent --id agent-1 --personality "Sarah Chen" \
  --focus "MCP protocol integration" \
  --week 1 --tasks 5 \
  --output /shared/goxel-simplified/handoffs/week1/sarah_chen_handoff.json

# Agent-2 (Michael - Systems Engineer)  
/launch-agent --id agent-2 --personality "Michael Rodriguez" \
  --focus "Daemon infrastructure" \
  --week 1 --tasks 4 \
  --output /shared/goxel-simplified/handoffs/week1/michael_rodriguez_handoff.json

# Agent-3 (Alex - QA Engineer)
/launch-agent --id agent-3 --personality "Alex Kumar" \
  --focus "Testing and performance" \
  --week 1 --tasks 4 \
  --output /shared/goxel-simplified/handoffs/week1/alex_kumar_handoff.json

# Agent-4 (David - Migration Specialist)
/launch-agent --id agent-4 --personality "David Park" \
  --focus "Migration and compatibility" \
  --week 1 --tasks 4 \
  --output /shared/goxel-simplified/handoffs/week1/david_park_handoff.json

# Agent-5 (Lisa - Documentation Lead)
/launch-agent --id agent-5 --personality "Lisa Thompson" \
  --focus "Documentation and integration" \
  --week 1 --tasks 3 \
  --output /shared/goxel-simplified/handoffs/week1/lisa_thompson_handoff.json
```

## Week 1 Success Criteria

### Quantitative Metrics
- [ ] 20 total tasks completed (4 per agent average)
- [ ] 100% agents deliver handoff documents
- [ ] Baseline performance measured
- [ ] All blocking dependencies resolved for Week 2

### Qualitative Metrics
- [ ] Clear architecture design consensus
- [ ] No major risks unidentified
- [ ] All agents understand dependencies
- [ ] Communication channels established

## Handoff Protocol

Each agent must produce a structured handoff at week's end:

```json
{
  "agent_id": "agent-1",
  "agent_name": "Sarah Chen",
  "week": 1,
  "completed_tasks": [
    {
      "task": "Document current data flow",
      "output": "analysis/data_flow.md",
      "blockers_for_others": []
    }
  ],
  "discoveries": [
    "Found opportunity to reduce serialization by 50%"
  ],
  "handoffs": {
    "agent-2": ["Socket integration points defined in src/daemon/mcp_integration.h"],
    "agent-3": ["Test interfaces ready in tests/mcp/interfaces.h"],
    "agent-4": ["Breaking changes documented in migration/breaking_changes.md"]
  },
  "next_week_dependencies": [
    "Need Michael's socket modifications by Day 8"
  ]
}
```

## Daily Sync Simulation

```markdown
# Daily Standup - Day 3
**Sarah**: MCP mapping 70% complete. Found 5 methods needing special handling.
**Michael**: Daemon profiling done. Startup is 450ms - need optimization.
**Alex**: Dashboard live at http://metrics.local. Tracking 15 metrics.
**David**: User survey sent. 2 responses highlighting WebSocket concerns.
**Lisa**: Wiki structure complete. Need architecture diagram from Sarah.

**Blockers**: None
**Helps Needed**: Lisa needs 30min with Sarah for diagram walkthrough
```

## Risk Monitoring

### Early Warning Signs (Week 1)
- Any agent not delivering daily updates
- Discovery of major architectural conflicts
- Performance baseline >2x worse than expected
- User pushback on migration plan

### Escalation Protocol
1. **Hour 24**: No update from agent → Lead (Sarah) checks in
2. **Hour 48**: Blocking issue → All-hands sync meeting
3. **Day 5**: Behind schedule → Redistribute tasks
4. **Week 1 End**: Major risk identified → Adjust Week 2 plan

---

**Launch Date**: January 29, 2025
**First Sync**: Day 1, 2:00 PM
**Week 1 Review**: Friday, 3:00 PM
**Handoff Deadline**: Friday, 5:00 PM