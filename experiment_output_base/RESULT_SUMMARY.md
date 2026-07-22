# PPSP 14,000-run result summary

Results use 70 probability-normalized scenarios, 20 paired common-random-number replications, and 10 policies. Monetary units are KRW million.

|Rank|Policy|Mean objective|Std.|CVaR 10%|Fail rate|P0 paired difference|95% clustered CI|
|---:|---|---:|---:|---:|---:|---:|---:|
|1|P8|153,881.8|23,826.9|120,696.0|75.9%|1,993.5|[1,458.4, 2,528.5]|
|2|P1|152,876.7|23,912.8|120,661.1|77.2%|988.4|[393.4, 1,583.3]|
|3|P5|152,584.2|24,472.6|118,210.5|80.1%|695.9|[-83.0, 1,474.8]|
|4|P3|152,546.3|22,650.8|121,031.4|75.6%|658.0|[92.4, 1,223.5]|
|5|P2|152,134.7|22,940.2|120,408.4|77.6%|246.4|[-236.0, 728.7]|
|6|P0|151,888.3|22,404.5|121,349.5|74.3%|0.0|[0.0, 0.0]|
|7|P6|151,138.8|22,680.9|118,376.8|78.8%|-749.5|[-1,501.9, 2.9]|
|8|P7|142,129.2|19,697.0|115,014.9|64.9%|-9,759.1|[-10,846.8, -8,671.5]|
|9|P4|122,280.0|17,804.6|95,323.7|98.1%|-29,608.3|[-31,118.3, -28,098.4]|
|10|P9|108,238.4|17,434.6|79,536.7|76.6%|-43,650.0|[-45,443.3, -41,856.7]|

## Interpretation boundaries

- P8 has the highest probability-weighted mean objective in this implementation; P0 has a slightly stronger lower-tail CVaR than P8.
- P7 has the lowest estimated fail rate, so the policy preferred by mean performance is not the policy preferred by operational reliability.
- P9 has the lowest dispersion but also the lowest mean objective; lower variance alone is not evidence of dominance.
- Failure is triggered by the supplied shortage-rate, consecutive-shortage, and minimum-profit rules. High failure rates must be discussed as a model result and checked through sensitivity analysis, not hidden.
- Confidence intervals cluster the 20 paired replications within each of the 70 scenarios; they do not treat 1,400 rows as unrelated observations.
