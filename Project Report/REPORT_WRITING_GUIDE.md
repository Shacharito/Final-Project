# Project Report — Writing Guide

Source of truth for structure/rules: the official blank template
`/mnt/vmshared/Template_Project_Report.docx` (course-provided, Hebrew instructions page + English body).
Our working draft: `Template Project Report (eng).docx` in this folder.

## Submission Rules
- Submit on Moodle after advisor approval, by the published deadline. Late submission = cannot
  present on the scheduled date, must reschedule.
- Must follow the attached template format exactly (auto-numbered headings, figures, tables, ToC).
- English is allowed, as long as the template format/styles are kept.
- Max file size: **10 MB**. Recommended length: **~20 pages**.
- **No source code printouts** in the report body. If specific code needs discussion, attach only
  the relevant snippet as an **Appendix**.
- Consult the advisor on which chapters actually apply to a project of this nature.

## Required Chapter Structure (in order)
1. **Abstract** (1 page) — topic, purpose, deliverable + a block diagram figure.
2. **Introduction** — goals, motivation, approach, comparison to existing work/algorithms.
3. **Theoretical background** — concepts + algorithms used, including alternatives considered
   and why they were rejected.
4. **Simulation** (or design) — simulation/design environment description + results.
5. **Implementation**
   - 5.1 Hardware Description — components, tools, platforms, diagrams.
   - 5.2 Software Description — tools + explanation (no code dumps).
6. **Analysis of results** — simulation vs. real-time vs. alternative algorithm, **emphasis on
   quantitative requirements** (numbers, tables, measured data).
7. **Conclusions and further work** — ⚠️ *most important chapter*: goals vs. actual results,
   improvement suggestions, future work possibilities.
8. **Project Documentation** — description of deliverables (hardware/software/sim/user guide),
   GitHub link, description of project files. (Describe the docs, don't paste their content.)
9. **References** — scientific citation style (books, papers, datasheets, app notes, user
   guides, links — see template for exact citation formats).
- **Appendices** — one per subject needing deep detail (e.g., a schematic, a datasheet excerpt,
  a code snippet that's specifically discussed).

## Writing Style Requirements (our own bar, on top of the template)
- Academic engineering tone: short, precise sentences. No filler/marketing language.
- Every design choice must state the **trade-off** (what was gained, what was given up).
- Every problem/limitation should include **root cause**, not just a symptom description.
- Prefer quantitative statements ("brownout at 1.76MB image size, boots fine ≤1MB") over vague
  ones ("the system sometimes had power issues").

## Current Draft Status (2026-07-02)
Chapters 1–5 (through 4.3 Integration Process) are drafted with real project-specific content.
Chapters 6–9 + Appendices are still the generic template placeholder text — they need real
measured data (see `project_report_missing_items.xlsx` for the full outstanding-tasks list).
