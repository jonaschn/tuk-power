import sys

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

tkey = "Time in ns"

filename = sys.argv[1] if len(sys.argv) > 1 else 'benchmark.csv'
data = pd.read_csv(filename)
dtype_dfs = []
for dtype in np.unique(data['Data type']):
    dtype_dfs.append(data[data['Data type'] == dtype])

for df in dtype_dfs:
    bandwidth = df['Column size in KB'] / df[tkey] / 1024 / 1024 * 1e9
    plt.plot(df['Column size in KB'], bandwidth, label=df['Data type'].iloc[0])
plt.legend()
plt.xlabel('Attribute Vector Size (in KB)')
plt.xscale('log')
plt.ylabel('Effective Scan Bandwidth (in GB/s)')
plt.ylim(ymin=0)
plt.legend(loc=2, prop={'size': 10})
plt.savefig(filename.replace('.csv','.png'))
plt.clf()
