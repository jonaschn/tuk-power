import os
import sys

import matplotlib.pyplot as plt
from matplotlib import ticker
import numpy as np
import pandas as pd


tkey = 'Time in ns'
colszkey = 'Column size in KB'
dtypekey = 'Data type'
colors = ['#f6a800', '#af0039', '#dd630d', '#007a9e']


def process_file(filename, show_variance, only_64):
    data = pd.read_csv(filename)
    csizes = np.unique(data[colszkey])
    dtype_dfs = []

    fig, ax = plt.subplots()
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)

    if only_64:
        dtype_dfs.append(data[data[dtypekey] == "int64"])
    else:
        for dtype in np.unique(data[dtypekey]):
            dtype_dfs.append(data[data[dtypekey] == dtype])

    for idx, df in enumerate(dtype_dfs):
        bandwidth = df[colszkey] / df[tkey] / 1024 / 1024 * 1e9
        bandwidth_means = [np.mean(bandwidth[df[colszkey] == csz]) for csz in csizes]
        bandwidth_stds = [np.std(bandwidth[df[colszkey] == csz]) for csz in csizes]

        plt.errorbar(x=csizes,
             y=bandwidth_means,
             yerr=bandwidth_stds if show_variance else None,
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

    if system_type == 'intel':
        cache_sizes_in_kib = {
            'L1': 256, # 8x 32 KiB/core
            'L2': 2048, # 8x 256 KiB/core
            'L3': 38400 # 37,5 MiB (shared)
        }
    else: # POWER 8 node with 12 cores
        cache_sizes_in_kib = {
            'L1': 768, # 12x 64 KiB/core
            'L2': 6144, # 12x 512 KiB/core = 6MiB
            'L3': 98304 # 12x 8192 KiB/core = 96MiB (shared)
        }

    # show cache sizes of L1, L2 and L3
    for cache in cache_sizes_in_kib:
        plt.axvline(cache_sizes_in_kib[cache], color='k', alpha=.3)
        plt.text(cache_sizes_in_kib[cache] * 0.4, plt.ylim()[1], cache, color='k', alpha=.3)

    # print labels in the right order
    handles, labels = plt.gca().get_legend_handles_labels()
    order = [2, 1, 0, 3]
    if only_64:
        plt.legend([handles[0]], [labels[0]], loc=2, prop={'size': 10})
    else:
        plt.legend([handles[i] for i in order], [labels[i] for i in order], loc=2, prop={'size': 10})

    caching_enabled = "caching1" in filename
    prefetching_enabled = "prefetch1" in filename
    column_store = "colstore" in filename
    caching_title = "With Caching" if caching_enabled else "No Caching"
    prefetching_title = "With Prefetching" if prefetching_enabled else "No Prefetching"
    store_title = "Column Store" if column_store else "Row Store"
    plt.title("{} - {} - {} - ".format(store_title, caching_title, prefetching_title), y=1.08)
    plt.savefig(filename.replace('.csv', '.png'))
    plt.clf()


if __name__ == '__main__':
    path = sys.argv[1] if len(sys.argv) > 1 else 'benchmark.csv'
    system_type = sys.argv[2] if len(sys.argv) > 2 else 'power'
    flags = sys.argv[3:]
    show_variance = 'no-variance' not in flags
    only_64 = 'only-64' in flags
    try:
        if os.path.isdir(path):
            for filename in os.listdir(path):
                if filename[-4:] == '.csv':
                    print('Plotting ' + filename + '...')
                    process_file(os.path.join(path, filename), show_variance, only_64)
        else:
            process_file(path)
        print('Done')
    except FileNotFoundError as e:
        print(e)
        print('Usage: python generate_figures.py path [machine]')
