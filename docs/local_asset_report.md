# Local Asset Report

`Scripts/report_local_project_status.ps1` is a local decision-aid for the Unreal
project copy, especially `C:\Unreal Projects\m7at10_dt`.

The script separates known local decision points from unexpected untracked files.
This keeps generated outputs, vendor/sample content, binary WBP edits, and local
settings out of code commits until each item has an explicit project decision.
The active decision list and completion evidence are tracked in
`docs/remaining_work.md`.

## Basic Usage

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1"
```

Useful machine-readable output:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -Json
```

## Gate Options

Fail when generated or local-output items are present:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnGeneratedOutput
```

Fail when specific decision categories are present:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnCategory LargeContentCandidate,SampleOrThirdParty
```

Fail when Git sees an untracked file that is not covered by the known decision
point inventory:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnUnclassifiedUntracked
```

Fail when a known decision-point path has already been staged:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnStagedDecisionPoints
```

## Current Categories

- `ReviewCandidate`: small project or binary edits that need manual review.
- `LargeContentCandidate`: Unreal content folders that may be valid production
  assets, but should be committed only after an explicit asset/vendor decision.
- `SampleOrThirdParty`: sample or third-party content that should not enter the
  project accidentally.
- `GeneratedOutput`: packaged outputs such as `Windows/` and `Windows.zip`.
- `GeneratedOrLocalConfig`: local runtime configuration.

`Config/Game.ini` receives an additional detected note when it contains
`[DTCoreRuntimeOverride]`. If every override value is blank, treat it as a
local-only runtime override unless shared endpoint defaults are explicitly
required. If any endpoint or credential value is populated, review it for
environment or secret leakage before staging.

Static runtime-config readiness:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_runtime_config_policy.ps1"
```

This check passes when `Config/Game.ini` is absent or its
`[DTCoreRuntimeOverride]` values are empty, and fails if local endpoint or
credential values are present.

`Content/M7AT10/UI/WBP_VirtualSensorMonitor.uasset` receives an additional
detected note because it is a binary Designer widget. Keep it untracked until it
has been opened in Unreal Editor, optional bindings have been checked against
`docs/widget_designer_setup.md`, and a PIE smoke pass confirms that the widget
does not crash or show stale sensor status.

Large content candidates and sample/third-party folders include extension counts
and the largest files in each folder. Use this to spot built-data-heavy map
packages, oversized textures, vendor packs, and copied sample projects before
deciding whether the repository should own them.

Every decision point also reports:

- `GitState`: `Untracked`, `Staged`, `TrackedModified`, or `CleanOrIgnored`.
- `CommitReadiness`: `BlockedByManualDecision`,
  `DoNotCommitGeneratedOutput`, or `NotPresent`.
- `ReviewQueue`: `ReadyToStage`, `NeedsOwnerDecision`, `KeepLocal`, or
  `NotPresent`.
- `DecisionOwner`: required decision authority such as
  `AssetOwnerRequired`, `ProjectOwnerRequired`, `ConfigOwnerRequired`,
  `PackagingOwnerRequired`, or `NotApplicable`.
- `DecisionStatus`: unresolved/accepted state such as `PendingOwnerDecision`,
  `EvidencePending`, `AcceptedForRepository`, `KeepLocal`,
  `DoNotCommitGeneratedOutput`, or `NotPresent`.
- `EvidenceNeeded`: concrete evidence that must be complete before a path can
  move to `ReadyToStage`.
- `EvidenceStatus`: current evidence review state such as `NoEvidenceRecord`,
  `PartialEvidence`, `AcceptedButEvidenceIncomplete`, `ReadyEvidenceAccepted`,
  or `GeneratedOutput`.
- `EvidenceSatisfied`: true only when required evidence is complete and the
  decision has reviewer/date acceptance metadata.
- `DecisionChecklist`: manual evidence that must be collected before staging.

DecisionOwner does not mean ownership has been accepted. It is review-routing
metadata for who must make the decision.
EvidenceNeeded must be complete before ReadyToStage, and
`AcceptedForRepository` should only be used after the evidence is recorded.
ReadyToStage requires AcceptedForRepository. ReadyToStage requires complete evidence. AcceptedForRepository requires complete EvidenceNeeded.
PendingOwnerDecision remains NeedsOwnerDecision. EvidencePending remains NeedsOwnerDecision.
Recorded evidence must name reviewer, date, and source.
Generated output remains KeepLocal.

For large content, the checklist asks for asset source, license, production
dependency, size review, and storage/versioning strategy. For sample or
third-party folders, it asks for project ownership, license/redistribution
terms, and whether documentation is preferable to committing copied sample
files. For the monitor WBP, it asks for editor open, binding verification, PIE
smoke evidence, and production-WBP acceptance. For `Config/Game.ini`, it asks
for endpoint/credential review and runtime-config policy validation.

Static large-content decision readiness:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_large_content_decision_policy.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_large_content_decision_policy.ps1" -FailIfPresent
```

The first command summarizes local large-content and sample candidates. The
second command is a strict content-review gate and fails until ownership,
dependency, and size decisions are complete.

Export a review bundle:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_local_asset_decision_report.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_local_asset_decision_report.ps1" -MarkdownPath ".\Saved\Reports\local_asset_decisions.md" -JsonPath ".\Saved\Reports\local_asset_decisions.json"
```

The exporter turns the same local asset report into a Markdown/JSON review
bundle. By default it only prints to the console; pass output paths when you
intentionally want generated review artifacts under `Saved/Reports`.

The Markdown export includes `Ready To Stage`, `Needs Owner Decision`, and
`Keep Local` review queues. Known local asset decision points should normally
start in `NeedsOwnerDecision`; generated package outputs should appear in
`KeepLocal`; no binary WBP, local config, large content, or sample folder should
move to `ReadyToStage` until the checklist evidence is complete.

Create an editable evidence template when a path is ready for owner review:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_local_asset_decision_evidence_template.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_local_asset_decision_report.ps1" -EvidencePath ".\docs\local_asset_decisions.evidence.json"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_local_asset_decision_evidence_workflow.ps1"
```

The template uses schema `LocalAssetDecisionEvidenceV1` and records
`DecisionEvidence` for each present non-generated decision point. A path moves
to `ReadyToStage` only when the evidence file sets
`DecisionStatus = AcceptedForRepository`, every `EvidenceNeeded` item has
`Status = Complete` and a non-empty `Source`, and `AcceptedBy`, `AcceptedAt`,
and `EvidenceSource` are filled in. Large content decisions require
owner/source/license, dependency, size, and storage/versioning evidence before
they can be accepted.
`Scripts/validate_local_asset_decision_evidence_workflow.ps1` builds a
temporary git-backed project and verifies no-evidence, incomplete evidence,
blank reviewer/date/source, blank item source, pending decision, normalized path, `KeepLocal`,
duplicate normalized evidence path rejection, generated-output override,
complete accepted evidence, and Staged decision gate behavior. Duplicate normalized evidence paths are invalid because they can hide conflicting owner decisions.
The Staged decision gate must fail for blocked decision paths and pass for paths
that are `ReadyToStage` with complete accepted evidence.

## Recommended Workflow

1. Run the report before staging.
2. Resolve or explicitly accept each decision point.
3. Run with `-FailOnUnclassifiedUntracked` to catch newly created paths that the
   inventory does not know about yet.
4. Stage only intended source, docs, config, or content files.
5. Run with `-FailOnStagedDecisionPoints` before committing when local content
   decisions are still open.

## Readiness Wrapper

For regular local checks, use `Scripts/check_project_readiness.ps1`. It runs the
local asset report with `-FailOnUnclassifiedUntracked`, then runs the smoke test
script unless `-SkipSmoke` is passed.

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipBuild
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -SkipLocalAssetDecisionEvidenceWorkflow
```

Strict local-output gates can be enabled when a clean packaging state is needed:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnGeneratedOutput
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnStagedDecisionPoints
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnLargeContentCandidates
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnCategory LargeContentCandidate,SampleOrThirdParty
```

Runtime config decisions can be checked against a separate local Unreal project
root while keeping `ProjectRoot` pointed at the source repo docs and scripts:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_runtime_config_policy.ps1" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_runtime_config_policy.ps1" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_runtime_config_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"
```

The runtime config report emits `RecommendedDecision`. Empty
`[DTCoreRuntimeOverride]` values produce `KeepLocal` by default because blank
local override placeholders are not useful shared project defaults unless a
config owner explicitly accepts them.

Monitor WBP decisions can be inspected without mutating the binary asset:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -Json
```

The WBP decision report records binary metadata, Git state, setup-document
contract terms, `RecommendedDecision`, and an evidence draft. It cannot replace
Unreal Editor verification; it keeps editor-open, optional binding, PIE smoke,
and production acceptance evidence separate from file metadata.

Large content decisions can be summarized separately from generated package
outputs:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_large_content_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_large_content_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -Json
```

The large content report reuses `report_local_project_status.ps1` and focuses on
`LargeContentCandidate` plus `SampleOrThirdParty` paths. It records size,
extension counts, largest files, risk, `RecommendedDecision`, and evidence
drafts. Content over 1 GB and copied samples default to `KeepLocal` until
source/license/dependency/storage evidence is complete and accepted.
