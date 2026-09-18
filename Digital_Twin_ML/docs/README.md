# Machine learning documentation

Reference documentation for the surrogate and feasibility machine learning pipeline built on the C++ methanol plant digital twin.

- `01-problem-formulation.md`: Architecture, pipeline execution, headline results, and provenance.
- `02-sampling-and-dataset.md`: Parameter sampling, labeling, weighting, and storage schema.
- `03-surrogate.md`: Scalar surrogate model, loss formulation, validation, and serving guards.
- `04-sensitivity.md`: Global sensitivity analysis, Sobol indices, and diagnostic figures.
- `05-feasibility-classifier.md`: Binary feasibility gate, multiclass failure mode attribution, and boundary behavior.
- `06-operating-envelope.md`: Rigorous recycle dynamics, stoichiometric limits, compression duty, and non-convex boundaries.

## Pipeline structure

| Component | Code package | Documentation |
|---|---|---|
| Shared foundation | `common/` | 01, 02 |
| Scalar surrogate | `surrogate/` | 03, 04 |
| Feasibility classifier | `feasibility/` | 05 |
| Exploration figures | `explore.py` | 04, 06 |
| Plant optimizer | `optimize.py` | 03, 05 |

All datasets and trained model artifacts store fingerprint hashes connecting them directly to the compiled C++ twin build.

## List of figures

| Figure | Title | Chapter | Path |
|---|---|---|---|
| Figure 1 | Flowsheet architecture of the green methanol digital twin | 01 | `figures/01-co2-hydrogenation-flowsheet.svg` |
| Figure 2 | Parity plot for overall carbon dioxide conversion cross-validation | 03 | `figures/parity_co2_conversion_overall_1.png` |
| Figure 3 | Out-of-fold residual error distributions across all surrogate targets | 03 | `figures/residuals_summary_1.png` |
| Figure 4 | Feature importance ranking for unit production cost | 03 | `figures/importance_unit_cost_USD_per_t_1.png` |
| Figure 5 | Global Sobol sensitivity indices decomposing variance across parameters | 04 | `figures/sobol_indices_1.png` |
| Figure 6 | 1D parameter response curves for carbon dioxide conversion | 04 | `figures/response_co2_conversion_overall_1.png` |
| Figure 7 | 2D quantitative contour maps with operational failure boundaries | 04 | `figures/contour2d_1.png` |
| Figure 8 | 3D surrogate response surfaces for conversion and unit production cost | 04 | `figures/surface3d_1.png` |
| Figure 9 | Multi-objective Pareto frontier of unit cost versus annual production | 04 | `figures/pareto_1.png` |
| Figure 10 | Receiver Operating Characteristic (ROC) and Precision-Recall (PR) curves | 05 | `figures/feasibility_roc_pr_1.png` |
| Figure 11 | Multiclass failure-mode confusion matrix | 05 | `figures/feasibility_confusion_1.png` |
| Figure 12 | Design space feasibility maps across 2D operational slices | 05 | `figures/feasible_region_1.png` |
| Figure 13 | Chemical engineering operating envelope mapping carbon yield vs compression | 06 | `figures/03-operating-envelope_1.png` |
