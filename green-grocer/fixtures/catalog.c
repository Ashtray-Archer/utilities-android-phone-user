#include "catalog.h"

/* Bundled sample data. Each unit is one countable package, never loose weight.
 * NULL image keys deliberately select placeholders in the later renderer. */
static const gg_product products[] = {
    {GG_BANANAS, "Bananas", "1 bunch", 129, NULL},
    {GG_APPLE_BAGS, "Apples", "1 bag (3 lb)", 499, NULL},
    {GG_CEREAL, "Oat cereal", "1 box (12 oz)", 379, NULL},
    {GG_MILK, "Whole milk", "1 half gallon", 349, NULL},
    {GG_EGGS, "Large eggs", "1 dozen", 425, NULL},
    {GG_BREAD, "Whole wheat sandwich bread", "1 loaf (20 oz)", 259, NULL},
    {GG_RICE, "Long grain rice", "1 bag (5 lb)", 899, NULL},
    {GG_COFFEE, "Ground coffee", "1 bag (12 oz)", 1249, NULL}
};

const gg_catalog gg_sample_catalog = {
    .products = products,
    .count = sizeof products / sizeof products[0]
};
