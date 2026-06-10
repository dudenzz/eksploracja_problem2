# cython: language_level=3
# cython: boundscheck=False
# cython: wraparound=False
# cython: cdivision=True
# cython: nonecheck=False

from cpython.list cimport PyList_Append, PyList_GET_SIZE, PyList_GET_ITEM
from cpython.dict cimport PyDict_GetItem, PyDict_SetItem
from cpython.ref cimport PyObject
import itertools


cdef class FPNode:
    cdef public object item
    cdef public int    count
    cdef public FPNode parent
    cdef public FPNode next_link
    cdef public FPNode tail_link
    cdef public dict   children

    def __cinit__(self, object item, int count, FPNode parent):
        self.item      = item
        self.count     = count
        self.parent    = parent
        self.next_link = None
        self.tail_link = None
        self.children  = {}

    cdef inline FPNode get_child(self, object item):
        return self.children.get(item)

    cdef inline FPNode add_child(self, object item):
        cdef FPNode node = FPNode(item, 0, self)
        self.children[item] = node
        return node

cdef class FPTree:
    cdef public FPNode root
    cdef public dict   header
    cdef public dict   freq

    def __cinit__(self):
        self.root   = FPNode(None, 0, None)
        self.header = {}
        self.freq   = {}

    cdef inline void _link(self, object item, FPNode node):
        cdef FPNode head
        if item in self.header:
            head = self.header[item]
            head.tail_link.next_link = node
            head.tail_link = node
        else:
            self.header[item] = node
            node.tail_link = node

    cdef void insert(self, list items):
        cdef FPNode cur = self.root, child
        cdef object item
        cdef int i, n = PyList_GET_SIZE(items)
        for i in range(n):
            item  = <object>PyList_GET_ITEM(items, i)
            child = cur.get_child(item)
            if child is None:
                child = cur.add_child(item)
                self._link(item, child)
            child.count += 1
            cur = child

    cdef list prefix_path(self, FPNode node):
        cdef list path = []
        cdef FPNode cur = node.parent
        while cur.item is not None:
            PyList_Append(path, cur.item)
            cur = cur.parent
        return path

cdef dict _sort_freq

def _sort_key(object x):
    return (-_sort_freq[x], x)

cdef FPTree _build(list dataset, int min_sup):
    global _sort_freq
    cdef:
        dict freq = {}
        list trans, filtered
        object item
        FPTree tree
        int cnt

    for trans in dataset:
        for item in trans:
            if item in freq:
                freq[item] += 1
            else:
                freq[item] = 1

    freq = {k: v for k, v in freq.items() if v >= min_sup}

    tree      = FPTree()
    tree.freq = freq

    _sort_freq = freq

    for trans in dataset:
        filtered = [it for it in trans if it in freq]
        if not filtered:
            continue
        filtered.sort(key=_sort_key)
        tree.insert(filtered)

    return tree

cdef void _mine(FPTree tree, int min_sup, frozenset prefix, dict result):
    cdef:
        object item
        FPNode node
        list cond_dataset, path
        FPTree cond_tree
        int sup, cnt
        frozenset new_prefix

    for item in tree.header:
        sup  = 0
        node = tree.header[item]
        while node is not None:
            sup += node.count
            node = node.next_link

        if sup < min_sup:
            continue

        new_prefix = frozenset(prefix | {item})
        result[new_prefix] = sup

        cond_dataset = []
        node = tree.header[item]
        while node is not None:
            cnt = node.count
            if cnt > 0:
                path = tree.prefix_path(node)
                if path:
                    for _ in range(cnt):
                        PyList_Append(cond_dataset, path)
            node = node.next_link

        if not cond_dataset:
            continue

        cond_tree = _build(cond_dataset, min_sup)
        if cond_tree.header:
            _mine(cond_tree, min_sup, new_prefix, result)

cpdef list solve_cy(list dataset, int min_support_count, double min_confidence):
    cdef:
        dict itemset_dict = {}
        FPTree tree
        object item
        int sup, support, support_a
        double conf
        frozenset itemset, ant_fs, cons_fs
        list itemset_list, rules_out = []
        tuple antecedent
        Py_ssize_t sz, i
        PyObject *ptr

    tree = _build(dataset, min_support_count)

    for item, sup in tree.freq.items():
        itemset_dict[frozenset({item})] = sup

    _mine(tree, min_support_count, frozenset(), itemset_dict)

    for itemset, support in itemset_dict.items():
        sz = len(itemset)
        if sz < 2:
            continue

        itemset_list = list(itemset)

        for i in range(1, sz):
            for antecedent in itertools.combinations(itemset_list, i):
                ant_fs = frozenset(antecedent)

                ptr = PyDict_GetItem(itemset_dict, ant_fs)
                if ptr is NULL:
                    continue

                support_a = <int>(<object>(<PyObject*>ptr))
                if support_a <= 0:
                    continue

                conf = <double>support / <double>support_a
                if conf >= min_confidence:
                    cons_fs = itemset - ant_fs
                    PyList_Append(rules_out, {
                        'A':    list(ant_fs),
                        'B':    list(cons_fs),
                        'supp': support,
                        'conf': conf,
                    })

    return rules_out
