import os
import re
import sys

import matplotlib.pyplot as plt
from matplotlib import ticker
import numpy as np
import pandas as pd
import argparse

parser = argparse.ArgumentParser(description='Generate plots for the TuK benchmarks')

tkey = 'Time in ns'
colszkey = 'Column size in KB'
dtypekey = 'Data type'
threads_key = 'Thread Count'
colors = ['#f6a800', '#af0039', '#dd630d', '#007a9e']


def argsort(arr):
    return sorted(range(len(arr)), key=arr.__getitem__)


def natural_order(text):
    return tuple(int(c) if c.isdigit() else c for c in re.split(r'(\d+)', text))


def process_file(filename, show_variance, only_64, system_type):
    data = pd.read_csv(filename)
    csizes = np.unique(data[colszkey])

    fig, ax = plt.subplots()
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)

    dtype_cols = []
    for column in set(data.columns) - {tkey, colszkey}:
        if len(np.unique(data[column])) > 1:
            dtype_cols.append(column)

    if not dtype_cols:
        print('Not enough dimensions to group by. Therefore data type was chosen.')
        dtype_cols.append(dtypekey)

    groups = data.groupby(by=dtype_cols)

    for idx, (group, df) in enumerate(groups):
        bandwidth = df[colszkey] / df[tkey] / 1024 / 1024 * 1e9
        bandwidth_means = [np.mean(bandwidth[df[colszkey] == csz]) for csz in csizes]
        bandwidth_stds = [np.std(bandwidth[df[colszkey] == csz]) for csz in csizes]

        number_of_threads = int(df[threads_key][df.index[0]].split(' ')[0]) # max could be used as well

        label = '|'.join([str(val) for val in (group if isinstance(group, tuple) else (group,))])
        plt.errorbar(x=csizes,
                     #y=np.multiply(bandwidth_means, number_of_threads),
                     y=bandwidth_means,
                     yerr=bandwidth_stds if show_variance else None,
                     label=label,
                     color=colors[idx], alpha=0.7,
                     ecolor='gray', lw=2, capsize=5, capthick=2)

    plt.legend()
    plt.xlabel('Attribute Vector Size (in KiB)')
    plt.xscale('log', basex=10)

    def comma_seperators(x, pos):
        return "{:,}".format(int(x))

    plt.gca().xaxis.set_major_formatter(ticker.FuncFormatter(comma_seperators))
    plt.minorticks_off()
    plt.xlim(xmin=8)
    plt.gca().yaxis.grid(True, lw=.5, ls='--')
    plt.ylabel('Effective Scan Bandwidth (in GiB/s)')

    nocaching_enabled = "nocache" in filename
    prefetching_enabled = "prefetch1" in filename
    column_store = not "rowstore" in filename
    caching_title = "No Caching" if nocaching_enabled else "With Caching"
    prefetching_title = "With Prefetching" if prefetching_enabled else "No Prefetching"
    store_title = "Column Store" if column_store else "Row Store"
    plt.title("{} - {} - {}".format(store_title, caching_title, prefetching_title), y=1.08)


    if system_type == 'intel':
        if nocaching_enabled:
            plt.ylim(ymin=0, ymax=20)
        else:
            plt.ylim(ymin=0, ymax=60)
    else:
        plt.ylim(ymin=0, ymax=15)

    if system_type == 'intel': # Intel E7-8890 v2 node with 15 cores
        cache_sizes_in_kib = {
            'L1': 480, # 15x 32 KiB/core
            'L2': 3840, # 15x 256 KiB/core
            'L3': 38400 # 15x 2,5 MiB/core = 37,5 MiB (shared)
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

    order = argsort([natural_order(label) for label in labels])
    plt.legend([handles[i] for i in order], [labels[i] for i in order], loc=2, prop={'size': 10})

    plt.savefig(filename.replace('.csv', '.png'))
    plt.close()


if __name__ == '__main__':
    parser.add_argument('path', help='directory with files to process or single file')
    parser.add_argument('system', help='system to plot results for', choices=['intel', 'power'])
    parser.add_argument('--no-variance', help='hide the variance in plots', action='store_false', dest='variance')
    parser.add_argument('--only-64', help='whether to plot only results for int64', action='store_true')
    args = parser.parse_args()

    print(vars(args))
    try:
        if os.path.isdir(args.path):
            for filename in os.listdir(args.path):
                if filename[-4:] == '.csv':
                    print('Plotting ' + filename + '...')
                    filepath = os.path.join(args.path, filename)
                    process_file(filepath, args.variance, args.only_64, args.system)
        else:
            process_file(args.path, args.variance, args.only_64, args.system)
        print('Done')
    except FileNotFoundError as e:
        print(e)
