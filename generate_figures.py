import sys

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

tkey     = "Time in ns"
colszkey = 'Column size in KB'
dtypekey = 'Data type'

filename = sys.argv[1] if len(sys.argv) > 1 else 'benchmark.csv'
data = pd.read_csv(filename)

csizes = np.unique(data[colszkey])
dtype_dfs = []

for dtype in np.unique(data[dtypekey]):
    dtype_dfs.append(data[data[dtypekey] == dtype])

for df in dtype_dfs:
    bandwidth = df[colszkey] / df[tkey] / 1024 / 1024 * 1e9
    bandwidth_means = [ np.mean(bandwidth[df[colszkey] == csz]) for csz in csizes ]
    bandwidth_stds  = [  np.std(bandwidth[df[colszkey] == csz]) for csz in csizes ]

    plt.errorbar(x=csizes,
                 y=bandwidth_means,
                 yerr=bandwidth_stds,
                 label=df[dtypekey].iloc[0])

plt.legend()
plt.xlabel('Attribute Vector Size (in KB)')
plt.xscale('log', basex=2)
plt.gca().xaxis.grid(True)
plt.ylabel('Effective Scan Bandwidth (in GB/s)')
plt.ylim(ymin=0)
plt.legend(loc=2, prop={'size': 10})
plt.savefig(filename.replace('.csv','.png'))
plt.clf()
