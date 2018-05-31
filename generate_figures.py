import sys

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns

tkey = "Time in ns"
fig = plt.figure(1, figsize=(9, 6))
ax = fig.add_subplot(111)

filename = sys.argv[1] if len(sys.argv) > 1 else 'benchmark.csv'
data = pd.read_csv(filename)
dtype_dfs = []

bandwidths=[]
labels=[]
colsizes=[]

for dtype in np.unique(data['Data type']):
  dtype_dfs.append(data[data['Data type'] == dtype])

for df in dtype_dfs:
  bandwidths.append(df['Column size in KB'] / df[tkey] / 1024 / 1024 * 1e9)
  labels.append(df['Data type'].iloc[0])
  colsizes.append(df['Column size in KB'].iloc[0])

df["bandwidth"] = df['Column size in KB'] / df [tkey] / 1024 / 1024 * 1e9

fig, ax = plt.subplots()
plot = sns.factorplot(kind='box',
    y='bandwidth',
    x='Column size in KB',
    hue='Data type',
    data=df[df['Data type'] == 'int8'])
ax.legend(loc=2, prop={'size': 10})
plt.savefig(filename.replace('.csv','.png'))

#plt.legend()
#plt.xlabel('Attribute Vector Size (in KB)')
#plt.xscale('log')
#plt.ylabel('Effective Scan Bandwidth (in GB/s)')
#plt.ylim(ymin=0)
#plt.legend(loc=2, prop={'size': 10})
#plt.clf()
