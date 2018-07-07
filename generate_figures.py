import os
import re
from itertools import product

import matplotlib.pyplot as plt
from matplotlib import ticker, rc
import numpy as np
import pandas as pd
import argparse

rc('font', **{'family': 'sans-serif', 'sans-serif': ['Verdana']})
# rc('text', usetex=True)

parser = argparse.ArgumentParser(description='Generate plots for the TuK benchmarks')

tkey = 'Time in ns'
colszkey = 'Column size in KB'  # These are actually KiB.
dtypekey = 'Data type'
threads_key = 'Thread Count'
colors = ['#af0039', '#007a9e', '#dd630d', '#f6a800']
linestyles = ['-', '--']
red = '#af0039'
yellow = '#f6a800'
blue = '#007a9e'
orange = '#dd630d'


def argsort(arr):
    return sorted(range(len(arr)), key=arr.__getitem__)


def natural_order(text):
    return tuple(int(c) if c.isdigit() else c for c in re.split(r'(\d+)', text))


def process_file(filename, show_variance, only_64, system_type, ylim, multicore):
    data = pd.read_csv(filename)
    csizes = np.unique(data[colszkey])

    fig, ax = plt.subplots()
    fig.set_size_inches(10, 4.5)
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
        data_in_bytes = df[colszkey] * 1024
        bandwidth = data_in_bytes / df[tkey]  # GB/s, not GiB/s
        bandwidth_means = [np.mean(bandwidth[df[colszkey] == csz]) for csz in csizes]
        bandwidth_stds = [np.std(bandwidth[df[colszkey] == csz]) for csz in csizes]

        # Use to remove irregularity in prefetching plot
        # if group == ('Column store', 'Prefetching') or group == ('Prefetching', 'Column store'):
        # bandwidth_means[14] = np.mean(bandwidth_means)

        # Use adjusted means if we need to smooth the curve
        adjusted_means = np.mean(list(zip(bandwidth_means, bandwidth_means[1::], bandwidth_means[2::])), axis=1)
        adjusted_means = np.insert(adjusted_means, 0, bandwidth_means[0])
        adjusted_means = np.append(adjusted_means, bandwidth_means[-1])
        # otherwise
        # adjusted_means = bandwidth_means

        number_of_threads = int(df[threads_key][df.index[0]].split(' ')[0]) # max could be used as well

        # styles = [(linestyle, color) for linestyle, color in product(linestyles, colors)]
        styles = [  # ('#af0039', '--'),
                  (red, '-'), (blue, '-'), (orange, '-'), (yellow, '-'),
                  (red, '--'), (blue, '--'), (orange, '--'), (yellow, '--')
        ]
        label = '|'.join([str(val) for val in (group if isinstance(group, tuple) else (group,))])
        plt.errorbar(x=csizes,
                     # y=np.multiply(bandwidth_means, number_of_threads),
                     y=adjusted_means,
                     yerr=bandwidth_stds if show_variance else None,
                     label=label,  linestyle=styles[idx][1],
                     color=styles[idx][0], alpha=0.7,
                     ecolor='gray', lw=2, capsize=5, capthick=2)

    plt.xlabel('Attribute Vector Size')
    plt.xscale('log', basex=10)
    plt.xticks(plt.xticks()[0][:-1], family='sans-serif')
    plt.yticks(family='sans-serif')
    plt.legend()

    def xFuncFormatter(x, pos):
        if (x >= 1000000):
            value = int(x / 1000000)
            unit = "GB"
        elif (x >= 1000):
            value = int(x / 1000)
            unit = "MB"
        else:
            value = int(x)
            unit = "KB"
        return "{:,} {}".format(value, unit)

    plt.gca().xaxis.set_major_formatter(ticker.FuncFormatter(xFuncFormatter))
    plt.minorticks_off()
    plt.xlim(xmin=8)
    plt.gca().yaxis.grid(True, lw=.5, ls='--')
    plt.ylabel('Effective Scan Bandwidth (in GB/s)')

    prefetching_enabled = "prefetch1" in filename
    column_store = not "rowstore" in filename
    prefetching_title = "With Prefetching" if prefetching_enabled else "No Prefetching"
    store_title = "Column Store" if column_store else "Row Store"
    # plt.title("{} - {}".format(store_title, prefetching_title), y=1.08)

    plt.ylim(ymin=0, ymax=ylim)

    if system_type == 'intel':  # Intel E7-8890 v2 node with 15 cores
        cache_sizes_in_kib = {
            'L1': 15 * 32 if multicore else 32,  # 15x 32 KiB/core
            'L2': 15 * 256 if multicore else 256,  # 15x 256 KiB/core
            'L3': 38400  # 15x 2,5 MiB/core = 37,5 MiB (shared)
        }
    else: # POWER 8 node with 12 cores
        cache_sizes_in_kib = {
            'L1': 12 * 64 if multicore else 64,  # 12x 64 KiB/core
            'L2': 12 * 512 if multicore else 512,  # 12x 512 KiB/core = 6MiB
            'L3': 98304  # 12x 8192 KiB/core = 96MiB (shared)
        }

    # show cache sizes of L1, L2 and L3
    for cache in cache_sizes_in_kib:
        plt.axvline(cache_sizes_in_kib[cache] * 1024 / 1000, color='k', alpha=.7)
        plt.text(cache_sizes_in_kib[cache] * 0.6, plt.ylim()[1], cache, color='k', alpha=.7)

    # print labels in the right order
    handles, labels = plt.gca().get_legend_handles_labels()

    order = argsort([natural_order(label) for label in labels])
    plt.legend([handles[i] for i in order], [labels[i] for i in order], loc=2, prop={'size': 10})

    plt.savefig(filename.replace('.csv', '.png'), dpi=200)
    plt.close()


if __name__ == '__main__':
    parser.add_argument('path', help='directory with files to process or single file')
    parser.add_argument('system', help='system to plot results for', choices=['intel', 'power'])
    parser.add_argument('--no-variance', help='hide the variance in plots', action='store_false', dest='variance')
    parser.add_argument('--only-64', help='whether to plot only results for int64', action='store_true')
    parser.add_argument('--singlecore', help='move the cache bars to values for a single core', action='store_false',
                        dest='multicore')
    parser.add_argument('--ylim', help='The maximum of the y axis', type=int, default=350)
    args = parser.parse_args()

    print(vars(args))
    try:
        if os.path.isdir(args.path):
            for filename in os.listdir(args.path):
                if filename[-4:] == '.csv':
                    print('Plotting ' + filename + '...')
                    filepath = os.path.join(args.path, filename)
                    process_file(filepath, args.variance, args.only_64, args.system, args.ylim, args.multicore)
        else:
            process_file(args.path, args.variance, args.only_64, args.system, args.ylim, args.multicore)
        print('Done')
    except FileNotFoundError as e:
        print(e)
