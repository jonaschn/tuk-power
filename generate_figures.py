import os
import sys

import matplotlib.pyplot as plt
from matplotlib import ticker
import numpy as np
import pandas as pd


tkey = 'Time in ns'
colszkey = 'Column size in KB'
dtypekey = 'Data type'
colors = ['red', 'green', 'blue', 'cyan']


def process_file(filename, show_variance):
    data = pd.read_csv(filename)
    csizes = np.unique(data[colszkey])
    dtype_dfs = []

    fig, ax = plt.subplots()
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)

    for dtype in np.unique(data[dtypekey]):
        dtype_dfs.append(data[data[dtypekey] == dtype])

    for idx, df in enumerate(dtype_dfs):
        bandwidth = df[colszkey] / df[tkey] / 1024 / 1024 * 1e9
        bandwidth_means = [np.mean(bandwidth[df[colszkey] == csz]) for csz in csizes]
        bandwidth_stds = [np.std(bandwidth[df[colszkey] == csz]) for csz in csizes]

        if show_variance:
            plt.errorbar(x=csizes,
                         y=bandwidth_means,
                         yerr=bandwidth_stds,
                         label=df[dtypekey].iloc[0],
                         color=colors[idx], alpha=0.7,
                         ecolor='gray', lw=2, capsize=5, capthick=2)
        else:
            plt.errorbar(x=csizes,
                         y=bandwidth_means,
                         label=df[dtypekey].iloc[0],
                         color=colors[idx], alpha=0.7,
                         ecolor='gray', lw=2, capsize=5, capthick=2)

    plt.legend()
    plt.xlabel('Attribute Vector Size (in KB)')
    plt.xscale('log', basex=10)

    def comma_seperators(x, pos):
        return "{:,}".format(int(x))

    plt.gca().xaxis.set_major_formatter(ticker.FuncFormatter(comma_seperators))
    plt.minorticks_off()
    plt.xlim(xmin=8)
    plt.gca().yaxis.grid(True, lw=.5, ls='--')
    plt.ylabel('Effective Scan Bandwidth (in GB/s)')

    if system_type == 'intel':
        if 'caching1' in filename:
            plt.ylim(ymin=0, ymax=60)
        else:
            plt.ylim(ymin=0, ymax=20)
    else:
        plt.ylim(ymin=0, ymax=100)

    # show cache sizes of L1, L2 and L3
    if system_type == 'intel':
        cache_sizes_in_kib = {
            'L1': 32,
            'L2': 256,
            'L3': 38400
        }
    else:
        cache_sizes_in_kib = {
            'L1': 64,
            'L2': 512,
            'L3': 8192
        }
    for cache in cache_sizes_in_kib:
        plt.axvline(cache_sizes_in_kib[cache], color='k', alpha=.3)
        plt.text(cache_sizes_in_kib[cache] * 0.4, plt.ylim()[1], cache, color='k', alpha=.3)

    # print labels in the right order
    handles, labels = plt.gca().get_legend_handles_labels()
    order = [2, 1, 0, 3]
    plt.legend([handles[i] for i in order], [labels[i] for i in order], loc=2, prop={'size': 10})

    plt.savefig(filename.replace('.csv', '.png'))
    plt.clf()


if __name__ == '__main__':
    path = sys.argv[1] if len(sys.argv) > 1 else 'benchmark.csv'
    system_type = sys.argv[2] if len(sys.argv) > 2 else 'power'
    show_variance = sys.argv[3] != 'no-variance' if len(sys.argv) > 3 else True
    try:
        if os.path.isdir(path):
            for filename in os.listdir(path):
                if filename[-4:] == '.csv':
                    print('Plotting ' + filename + '...')
                    process_file(os.path.join(path, filename), show_variance)
        else:
            process_file(path)
        print('Done')
    except FileNotFoundError as e:
        print(e)
        print('Usage: python generate_figures.py path [machine]')
