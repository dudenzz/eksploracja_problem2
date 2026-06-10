import pandas as pd
import numpy as np
import config

from solve_cy import solve_cy


def solve(min_support, min_confidence, verbose=False):
    path = getattr(config, 'datapath', None)

    raw = pd.read_csv(
        path,
        usecols=['Invoice', 'StockCode'],
        dtype={'Invoice': str, 'StockCode': str},
        encoding='ISO-8859-1',
        engine='c',
    )

    mask = raw['Invoice'].str.isdigit().values & raw['StockCode'].notna().values
    in_v = raw['Invoice'].values[mask]
    st_v = raw['StockCode'].values[mask]
    del raw

    idx  = np.argsort(in_v, kind='stable')
    in_v = in_v[idx]
    st_v = st_v[idx]

    dataset = []
    curr_inv   = in_v[0]
    curr_items = {st_v[0]}
    for i in range(1, len(in_v)):
        if in_v[i] == curr_inv:
            curr_items.add(st_v[i])
        else:
            dataset.append(list(curr_items))
            curr_inv   = in_v[i]
            curr_items = {st_v[i]}
    dataset.append(list(curr_items))
    del in_v, st_v

    min_sup_count = max(1, int(min_support * len(dataset)))
    return solve_cy(dataset, min_sup_count, min_confidence)
