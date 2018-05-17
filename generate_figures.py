import sys

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

filename = sys.argv[1] if len(sys.argv) > 1 else 'benchmark.csv'
data = pd.read_csv(filename)
dtype_dfs = []
for dtype in np.unique(data['Data type']):
    dtype_dfs.append(data[data['Data type'] == dtype])

for df in dtype_dfs:
    bandwidth = df['Column size in KB'] / df['Time'] / 1024 / 1024
    plt.plot(df['Column size in KB'], bandwidth, label=df['Data type'].iloc[0])
plt.legend()
plt.xlabel('Attribute Vector Size (in KB)')
plt.xscale('log')
plt.ylabel('Effective Scan Bandwidth (in GB/s)')
plt.ylim(ymin=0)
plt.savefig('bandwidth.png')
plt.clf()

for df in dtype_dfs:
    plt.plot(df['Column size in KB'], df['Time'], label=df['Data type'].iloc[0])
plt.legend()
plt.xlabel('Attribute Vector Size (in KB)')
plt.xscale('log')
plt.ylabel('Time elapsed (in ns)')
plt.ylim(ymin=0)
plt.savefig('time.png')
