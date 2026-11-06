<!--
  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
  SPDX-License-Identifier: BSD-3-Clause-Clear
-->
# Repeatable Task-Based SVE Course Module Template

<!--
AUTHOR INSTRUCTIONS

Use this template to create a practical course module focused on one SVE
engineering task. Learners complete the task through a sequence of short
workflow steps.

This template defines the educational structure of an SVE course module. It
does not prescribe the engineering implementation. Adapt the workflow to the
engineering task while maintaining a consistent learner experience across the
course.

Start with the engineering assets already available for the task, such as the
README, source, tests, measurements, reports, and technical notes. Map that
material into the module workflow rather than recreating it from scratch.

Author-engineers should draft the complete technical and learner-facing content
required by the relevant sections, including explanations, commands, expected
results, checks, interpretation, conclusions, and assessment content. Editorial
development will help refine structure, clarity, sequencing, consistency, and
learning design. Author-engineers remain responsible for technical accuracy and
completeness.

Replace text in [SQUARE BRACKETS]. Delete author notes and unused optional
sections before publication.

Keep each module as one Markdown file during initial authoring. It may be split
into several files during development or publication.
-->

<details>
<summary><strong>FOR INFORMATION: Authoring principles</strong></summary>

### Use module, task, workflow, and workflow step consistently

- **Module** — the complete learner-facing unit.
- **Task** — the practical engineering problem completed within the module.
- **Workflow** — the sequence used to complete the task.
- **Workflow step** — one short, purposeful stage within the workflow.

A module should normally contain one main task. Alternative implementations,
optimisation stages, checks, and comparisons normally form part of the workflow
rather than becoming separate tasks.

### Keep the engineering task central

State the task in the module title and frame the engineering judgement through a
guiding question.

Across the workflow, learners should work directly with the relevant software,
make predictions where useful, check outputs and correctness, gather and compare
evidence, interpret trade-offs, and use the evidence to reach and apply a
conclusion.

The sequence should reflect the task rather than a fixed formula.

### Module-level content

Keep these elements once per module:

- module title stating the practical task and a guiding engineering question;
- real-world motivation;
- measurable learning outcomes;
- shared prerequisites and module preparation;
- complete workflow plan;
- repository and workflow assets;
- final evidence review and engineering conclusion;
- module assessment and applied scenario;
- module summary and further reading.

### Workflow-step content

Each workflow step should normally:

- take approximately 5–20 minutes, excluding long downloads, builds, or runs;
- advance the learner towards completing the module task;
- focus on one clear engineering action;
- state where commands run when ambiguity is possible;
- produce a check, observable result, or useful evidence;
- include any necessary correctness or quality checks;
- explain what the result means;
- end with a short takeaway or transition where useful.

Prediction, detailed analysis, comparison tables, and takeaways are not required
in every workflow step. Use them where they support meaningful engineering
reasoning.

### Use the project workflow without hiding the teaching point

Use the project's documented build, run, test, measurement, and reporting
workflows. Learners should still be able to identify:

- the source, function, or configuration containing the important change;
- the inputs and parameters used;
- how expected behaviour is checked;
- how outputs or measurements are produced;
- which assumptions and limitations affect the result.

Use focused excerpts where they contain a teaching point. Do not narrate every
line of a source file, configuration file, or orchestration tool.

### Author the complete module draft

Author-engineers should complete the relevant learner-facing sections rather
than supplying technical notes for someone else to convert into a module.
Editorial development may restructure, clarify, shorten, or strengthen the
draft, but it does not replace subject-matter authorship or technical review.

### Heading hierarchy

- `#` — module title and major module sections;
- `##` — numbered workflow steps or major subsections;
- `###` — actions, predictions, checks, evidence, and analysis within a workflow step;
- `####` — supporting detail used sparingly.

Choose H1 or H2 for workflow sections and steps according to the module's
structure.

Use content-specific headings where possible. The only generic headings should
normally be:

At H1: `Introduction`, `Before you begin`, `Check your understanding`

At H2: `About this module`, `What will you be able to do?`

</details>

---

# AUTHORING

## Module information

<!-- Use front matter instead if required by the publishing system. -->

| Item | Details |
|---|---|
| Estimated time | [TOTAL ACTIVE TIME, EXCLUDING OPTIONAL WORK AND LONG BUILDS OR RUNS] |
| Target audience | [LEARNER PROFILE] |
| Prerequisite knowledge | [REQUIRED KNOWLEDGE AND SKILLS] |
| Previous module or dependencies | [DEPENDENCIES OR NONE] |
| Tested platform or environment | [TESTED ENVIRONMENT] |
| Required architecture features | [REQUIRED FEATURES] |
| Tested software and toolchain | [IMPORTANT VERSIONS] |
| Important configuration | [SETTINGS THAT AFFECT THE RESULT] |

# Module [NUMBER]: [ACTION-LED MODULE TITLE]

### [GUIDING ENGINEERING QUESTION?]

> This question should set the scene and will be answered in the module conclusion.

<!--
Good titles identify the workload, action, and target where relevant.

Good questions require evidence and judgement, for example:
- When should [APPROACH A] be used instead of [APPROACH B]?
- How does [CHANGE] affect [WORKLOAD] behaviour on the tested target?
- Which approach best meets [APPLICATION REQUIREMENT]?
- Where does [TECHNIQUE] help, and which limitation remains?
-->

# Introduction

## About this module

<!--
Introduce the real-world problem. Explain:
- where the workload or technique is used;
- why it matters;
- what makes it an interesting engineering problem;
- how it connects to prior or later learning where relevant.

Avoid detailed implementation discussion here.
-->

In this module, you will complete the task of [PRACTICAL ENGINEERING TASK].

You will work with:

| Path, stage, or implementation | Purpose in the module |
|---|---|
| [REFERENCE, CURRENT STATE, OR STARTING POINT] | [PURPOSE] |
| [FIRST CHANGE, ALTERNATIVE, OR TECHNIQUE] | [PURPOSE] |
| [OPTIONAL ADDITIONAL STAGE OR PATH] | [PURPOSE] |

<!--
Use a short list instead of a table when that reads more naturally.

State what remains fixed where the module compares paths, stages, or results.
Relevant controls may include inputs, workload, target, configuration,
measurement method, or application requirement.
-->

The aim is not to assume that [ONE APPROACH OR RESULT] is always best. You will
use [CORRECTNESS, PERFORMANCE, PORTABILITY, MAINTAINABILITY, COMPLEXITY,
INTEGRATION, OR OTHER EVIDENCE] to decide [WHAT THE LEARNER WILL DECIDE].

## What will you be able to do?

<!--
Select and adapt only the outcomes that are material to this module. Do not
retain an outcome simply because it appears in the template.

Use measurable verbs. Avoid stand-alone outcomes such as "understand".
-->

By the end of this module, you will be able to:

- [PREPARE, BUILD, OR RUN THE STARTING POINT];
- [INSPECT, PROFILE, OR EXPLAIN AN IMPORTANT PROPERTY];
- [APPLY, MODIFY, OR EVALUATE A RELEVANT TECHNIQUE];
- [CHECK CORRECTNESS OR REQUIRED BEHAVIOUR];
- [GATHER AND INTERPRET RELEVANT EVIDENCE];
- [EXPLAIN AN IMPORTANT TRADE-OFF OR LIMITATION];
- [TRANSFER THE ENGINEERING METHOD TO A RELATED CASE];
- [USE THE EVIDENCE TO RECOMMEND AN APPROACH OR NEXT STEP].

# Before you begin

## [ACTION VERB] [WORKLOAD OR CORE CONCEPT]

<!--
Provide only the background required to begin the module. Explain the important
inputs, outputs, processing stages, assumptions, and likely engineering
constraints.

Introduce supporting concepts here or at the point of need in the workflow.
Include enough technical context in the draft for an editor and reviewer to
assess completeness; excess detail can be reduced later.

Use optional or collapsible background reading when a concept is useful but not
required to complete the task.
-->

[BACKGROUND CONTENT]

## Prerequisites

Before starting, you need:

- [REQUIRED PLATFORM OR EXECUTION ENVIRONMENT];
- [REQUIRED SOFTWARE OR TOOLCHAIN];
- [REQUIRED ACCOUNTS, ACCESS, OR INPUTS];
- [EXPECTED DISK, MEMORY, NETWORK, BUILD, OR RUN REQUIREMENTS];
- [ASSUMED KNOWLEDGE].

<details>
<summary><strong>OPTIONAL: Development and target environments</strong></summary>

<!--
Include this section only when work is split between environments. Delete the
second row if everything runs in one environment.
-->

| Environment | Responsibility | Main outputs |
|---|---|---|
| Development environment | [PREPARE, BUILD, CONTROL, OR INSPECT] | [OUTPUTS] |
| Target environment | [RUN, TEST, PROFILE, OR MEASURE] | [OUTPUTS] |

Label every command group with its execution environment whenever ambiguity is
possible.

</details>

## Prepare for the module

<!--
Link to shared course setup where possible. Do not repeat course-wide setup in
every module.

Include only preparation and verification needed for this module. Use the
project's documented workflow rather than introducing an unnecessary wrapper.
-->

Run on the [ENVIRONMENT] from [STARTING LOCATION]:

```bash
[PREPARATION COMMANDS]
```

Verify the setup:

```bash
[VERSION, ARCHITECTURE, FEATURE, CONNECTION, BUILD, OR CAPABILITY CHECK]
```

You should expect output similar to:

```text
[ILLUSTRATIVE OUTPUT]
```

Reference output is illustrative. [STATE WHICH VALUES MAY DIFFER AND WHY].

Before continuing, confirm:

- [REQUIRED TOOL, FEATURE, OR VERSION];
- [REQUIRED INPUT, DATA, OR ASSET];
- [REQUIRED CONNECTION OR PERMISSION];
- [EXPECTED PROJECT LOCATION OR STARTING STATE].

# Plan the [WORKFLOW NAME] workflow

<!--
The list below illustrates common workflow functions. It is not a required
sequence. Include only the steps needed to complete this module's task, and use
content-specific wording.
-->

You will:

1. [INSPECT OR PREPARE THE WORKLOAD].
2. [ESTABLISH AND CHECK THE STARTING POINT].
3. [GATHER EVIDENCE ABOUT AN IMPORTANT PROPERTY, IF RELEVANT].
4. [APPLY, ENABLE, OR EXAMINE A CHANGE].
5. [CHECK CORRECTNESS OR REQUIRED BEHAVIOUR].
6. [COMPARE OR EVALUATE THE RELEVANT RESULTS, IF NEEDED].
7. [USE THE EVIDENCE TO ANSWER THE GUIDING QUESTION].

By the end of the workflow, you should have the following information or
artefacts:

- [STARTING-POINT RESULT OR ARTEFACT];
- [IMPORTANT SOURCE, CONFIGURATION, OR OBSERVATION];
- [CORRECTNESS OR QUALITY EVIDENCE];
- [MEASUREMENT, PROFILE, REPORT, OR OTHER EVIDENCE, IF RELEVANT];
- [COMPARISON OR INTERPRETATION, IF RELEVANT];
- [ENGINEERING CONCLUSION OR RECOMMENDATION].

## Repository and workflow assets

<!--
List only the assets learners need to locate or use. These may include source,
configuration, inputs, tests, executables, build targets, scripts, reports, or
generated results.
-->

| Path or asset | Purpose | Used in |
|---|---|---|
| `[PATH OR ASSET]` | [PURPOSE] | [ENVIRONMENT OR WORKFLOW STEP] |
| `[PATH OR ASSET]` | [PURPOSE] | [ENVIRONMENT OR WORKFLOW STEP] |
| `[PATH OR ASSET]` | [PURPOSE] | [ENVIRONMENT OR WORKFLOW STEP] |

[Explain which assets are inputs, which are generated, and how relevant outputs
move between environments or workflow steps.]

---

# Complete the [WORKFLOW NAME] workflow

<!--
Copy and adapt the following block for each workflow step. Use only the sections
relevant to that step. Prediction, detailed analysis, comparison tables, and
takeaways are not required in every step.
-->

## 1. [CONTENT-SPECIFIC WORKFLOW-STEP TITLE]

[Explain briefly why this step is needed, how it contributes to completing the
module task, and what result or evidence it will produce.]

### Prepare for this step

<!-- Include only step-specific preparation. Delete if none is required. -->

Run on the [ENVIRONMENT]:

```bash
[STEP-SPECIFIC PREPARATION]
```

### Predict [STEP-SPECIFIC OUTCOME]

<!--
Include a prediction only when learners have enough information to make a
meaningful judgement before seeing the result.
-->

Before running the step, consider:

| Prediction question | Your prediction | Reason |
|---|---|---|
| [WHAT RESULT OR CHANGE DO YOU EXPECT?] | | |
| [WHICH FACTOR MAY DOMINATE?] | | |
| [WHAT LIMITATION MAY REMAIN?] | | |

### Run [CONTENT-SPECIFIC ACTION]

Run on the [ENVIRONMENT] from [STARTING LOCATION]:

```bash
[COMMAND]
```

You should expect output similar to:

```text
[REFERENCE OUTPUT]
```

Reference output is illustrative. [STATE WHICH VALUES MAY DIFFER AND WHY].

### Inspect or interpret [IMPORTANT CODE, CONFIGURATION, OUTPUT, OR RESULT]

<!--
Reproduce only the source, configuration, graph, or output fields that contain a
teaching point. Name the file, function, or artefact where relevant. Do not
narrate every line.

Place this section before execution when learners need it to make a meaningful
prediction or run the step safely. Place it after execution when it primarily
explains an observed result.
-->

```[LANGUAGE OR FORMAT]
[FOCUSED EXCERPT]
```

[Explain how the excerpt connects inputs to outputs, why it matters to this
step, and which concepts the learner needs at this point.]

### Check [CORRECTNESS, ARTEFACT, OR BEHAVIOUR]

Before continuing, confirm:

- [EXPECTED FILE, PROCESS, OUTPUT, OR STATE];
- [CORRECTNESS, QUALITY, OR BEHAVIOUR REQUIREMENT];
- [EXPECTED CONFIGURATION OR TARGET].

If [COMMON FAILURE] occurs, [RECOVERY STEP OR TROUBLESHOOTING REFERENCE].

### Show or record the result

<!--
Use this section where the step produces evidence worth recording or comparing.
A simple confirmation may be sufficient for setup or inspection steps.

Where a workflow step introduces a material performance hypothesis, authors
should normally show a comparable result before and after the change, where this
is meaningful and practical. Explain what changed, why it was expected to affect
performance, and what the evidence shows.
-->

| Evidence or observation | Result | Notes |
|---|---|---|
| [STEP-SPECIFIC RESULT] | [VALUE OR DESCRIPTION] | |
| [STEP-SPECIFIC RESULT] | [VALUE OR DESCRIPTION] | |

### Analyse [STEP-SPECIFIC RESULT]

<!--
Help learners interpret what they observed. Explain why the result may differ
from their prediction, which part of the workflow produced the effect, and what
limitations remain.

Focus on engineering judgement rather than simply presenting the answer. Scope
claims to the tested workload, inputs, target, configuration, and versions.

Where the step compares results, explain what was held fixed and identify any
remaining uncontrolled variables.
-->

[ANALYSIS]

### Workflow-step takeaway

<!-- Delete where a takeaway or transition would be repetitive. -->

[ONE OR TWO SENTENCES STATING WHAT THIS STEP ESTABLISHED AND WHAT COMES NEXT.]

<!-- Repeat the workflow-step block as required. -->

<details>
<summary><strong>OPTIONAL AUTHOR GUIDANCE: Comparing results within a workflow step</strong></summary>

Use a controlled comparison only when it helps answer the guiding question.
Select metrics that are meaningful for the task rather than retaining generic
rows.

| Metric or criterion | [STARTING POINT] | [ALTERNATIVE OR LATER STAGE] | Difference or interpretation |
|---|---:|---:|---|
| Correctness or required behaviour | [VALUE] | [VALUE] | [INTERPRETATION] |
| [TASK-SPECIFIC METRIC] | [VALUE] | [VALUE] | [INTERPRETATION] |
| [ENGINEERING COST OR LIMITATION] | [VALUE OR DESCRIPTION] | [VALUE OR DESCRIPTION] | [INTERPRETATION] |

Record the relevant conditions of the comparison, such as the tested platform,
software versions, configuration, inputs, repetitions, and summary method.

</details>

---

# Review the evidence from the workflow

<!--
Include this section when learners need to bring together evidence from several
workflow steps before moving to the module conclusion. Use only the questions
relevant to the module.
-->

Review the evidence you collected during the workflow.

| Question | Your conclusion |
|---|---|
| What does the evidence show? | |
| Which evidence is most important? | |
| Which assumption, trade-off, or limitation affects the conclusion? | |
| What remains uncertain? | |
| What should be done or validated next? | |

Use your conclusions to form an evidence-based answer to the guiding question:

> [GUIDING ENGINEERING QUESTION]

[LEARNER'S CONCLUSION]

<details>
<summary><strong>OPTIONAL AUTHORING BLOCK: Generalise the method or state an engineering decision</strong></summary>

<!--
Use one or both of the following functions where they add value. They may be
included as separate, content-specific sections or incorporated into the final
module conclusion.
-->

## Generalise the [ENGINEERING METHOD OR TECHNIQUE]

The transferable method is:

1. [FIRST TRANSFERABLE ACTION].
2. [SECOND TRANSFERABLE ACTION].
3. [THIRD TRANSFERABLE ACTION].
4. [HOW TO CHECK THE RESULT].
5. [HOW TO DECIDE WHETHER TO APPLY IT].

Consider this method for [RELATED WORKLOAD OR SOFTWARE PATTERN]. It may be less
appropriate when [LIMITING CONDITION].

## Decide how to apply the [TECHNIQUE, RESULT, OR APPROACH]

The evidence supports the following guidance:

- apply [OPTION OR APPROACH A] when [CONDITION];
- use [OPTION OR APPROACH B] when [CONDITION];
- collect [ADDITIONAL EVIDENCE] before [ENGINEERING OR PRODUCTION DECISION].

</details>

# Check your understanding

<!--
Include two or three reasoning-based questions and an applied scenario.
Questions may be single-answer or multiple-answer.

Focus on:
- engineering reasoning;
- interpretation of results;
- understanding of the method or technique;
- appropriate use of the workflow;
- recognition of assumptions and limitations.

Avoid testing only terminology, intrinsic names, commands, or filenames.
Provide feedback for each incorrect answer.
-->

**Question 1**

[QUESTION STEM]

A. [PLAUSIBLE OPTION]

B. [PLAUSIBLE OPTION]

C. [PLAUSIBLE OPTION]

D. [PLAUSIBLE OPTION]

<details>
<summary>Answer</summary>

**[LETTER]. [CORRECT OPTION]**

[EXPLANATION BASED ON THE WORKFLOW EVIDENCE]

</details>

<details>
<summary>Incorrect-answer feedback</summary>

**A.** [TARGETED FEEDBACK]

**B.** [TARGETED FEEDBACK]

**C.** [TARGETED FEEDBACK]

**D.** [TARGETED FEEDBACK]

</details>

## [APPLIED DECISION SCENARIO]

<!--
Present a realistic scenario with a workload, target, constraints, and relevant
evidence. Change meaningful details from the completed workflow so the learner
must transfer the method rather than repeat an earlier answer.
-->

Which next step or engineering decision is best?

A. [OPTION]

B. [OPTION]

C. [OPTION]

D. [OPTION]

<details>
<summary>Answer</summary>

**[LETTER]. [CORRECT OPTION]**

[EXPLAIN HOW THE REQUIREMENT AND EVIDENCE SUPPORT THE DECISION]

</details>

<details>
<summary>Incorrect-answer feedback</summary>

**A.** [FEEDBACK]

**B.** [FEEDBACK]

**C.** [FEEDBACK]

**D.** [FEEDBACK]

</details>

# Apply the [MODULE-SPECIFIC] conclusion

<!--
Answer the guiding question using the evidence collected in the module.

Remind learners:
- what engineering task they completed;
- how the workflow progressed;
- which techniques or reusable concepts they encountered;
- what evidence they collected;
- what conclusion they reached;
- what the result does not prove and which validation remains.

This section may incorporate generalisation and decision content where that
provides a clearer ending. Retain the evidence, transfer, and decision functions
even when visible sections are combined.
-->

In this module, you:

- [PREPARED OR ESTABLISHED THE STARTING POINT];
- [INSPECTED OR ANALYSED AN IMPORTANT PROPERTY];
- [APPLIED, MODIFIED, OR EVALUATED A TECHNIQUE];
- [CHECKED CORRECTNESS OR REQUIRED BEHAVIOUR];
- [GATHERED AND INTERPRETED RELEVANT EVIDENCE];
- [ASSESSED AN IMPORTANT TRADE-OFF OR LIMITATION];
- [REACHED AN EVIDENCE-BASED ENGINEERING CONCLUSION].

[Answer the guiding question. Explain how the method or reusable concept can
transfer to other software. State what the results do not prove and which
validation remains.]

## Further reading

<!--
Include approved material that supports the module. Refer to related reusable
SVE programming concepts or patterns elsewhere in the course where appropriate.
Do not assume that those concepts will be packaged as a separate course.
-->

- [RELATED REUSABLE SVE CONCEPT OR PATTERN, IF AVAILABLE]
- [APPROVED TECHNICAL DOCUMENTATION OR REFERENCE]
- [APPROVED LEARNING PATH, GUIDE, OR ARTICLE]
- [NEXT COURSE MODULE]

---

# Author quality and release checklist

<!--
Author-engineers should use this checklist while drafting and technically
reviewing the module. Editorial development can refine the presentation but does
not replace SME-authored content or technical validation.
-->

## Structure and scope

- [ ] The module focuses on one practical engineering task and one guiding question.
- [ ] Existing engineering assets are mapped into the workflow rather than duplicated unnecessarily.
- [ ] The author-engineer has drafted the complete technical and learner-facing content.
- [ ] Each learning outcome is measurable and addressed within the workflow.
- [ ] Shared preparation appears once at module level.
- [ ] Each workflow step has one clear engineering purpose.
- [ ] The workflow follows the task rather than a forced formula.
- [ ] Learners review the relevant evidence before reading the final conclusion.
- [ ] Optional sections are included only where they add value.

## Commands and assets

- [ ] Every command states where it runs when ambiguity is possible.
- [ ] Commands have been tested from the documented starting state.
- [ ] Important versions, configurations, inputs, and dependencies are recorded.
- [ ] Inputs, source, generated outputs, and results are distinguished clearly.
- [ ] Commands are safe to rerun, or limitations are explained.
- [ ] Project workflows make execution repeatable without hiding the teaching point.
- [ ] Focused excerpts name the relevant file, function, or artefact.
- [ ] Reference output is labelled illustrative where values may vary.

## Workflow and SVE quality

- [ ] The workflow produces the evidence needed to answer the guiding question.
- [ ] Correctness or required behaviour is checked before performance is interpreted.
- [ ] Compiler behaviour is assessed where it materially supports the task.
- [ ] Relevant SVE, SVE2, or later feature requirements are identified accurately.
- [ ] Workload assumptions and relevant edge cases are tested or stated.
- [ ] Vector-length-agnostic claims are validated using the approved course workflow.
- [ ] Reusable SVE programming concepts or patterns are introduced or reinforced at the point of need.
- [ ] Analysis explains why results occurred and scopes claims to the tested configuration.
- [ ] Remaining limitations and required validation are stated clearly.

## Evidence, transfer, and assessment

- [ ] Comparisons hold relevant inputs and settings fixed, or explain what changed.
- [ ] Measurement context is recorded where quantitative results are used.
- [ ] Results cover a meaningful range of conditions where the conclusion depends on them.
- [ ] The reusable engineering method and its limits are stated where relevant.
- [ ] Knowledge checks test reasoning rather than memorisation.
- [ ] Incorrect-answer feedback addresses the misconception behind each option.
- [ ] The applied scenario changes meaningful details and requires transfer.
- [ ] The final section answers the guiding question and gives an appropriate engineering conclusion.

## Editorial and release checks

- [ ] Estimated time reflects the learner's active workload.
- [ ] Acronyms and specialist terms are defined on first use.
- [ ] Headings are useful in navigation, search, and independently linked pages.
- [ ] File names, paths, units, terminology, and capitalisation are consistent.
- [ ] Code blocks, tables, images, diagrams, and collapsible sections render correctly.
- [ ] Images have useful alt text and match the current workflow or interface.
- [ ] Links and downloads are accessible to the intended audience.
- [ ] Draft notes, placeholders, generated artefacts, private links, and credentials are absent.
- [ ] Remaining technical or production validation is stated clearly.
- [ ] Licensing, attribution, and code-source requirements are satisfied.
