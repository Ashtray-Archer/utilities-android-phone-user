# Compile the two tab-separated relations into the native model's row types.
BEGIN {
    FS = "\t"
    print "module StorefrontFixtures\n\nimport StorefrontTypes\n\nexport\ncatalog : List ShelfItem\ncatalog = ["
}
function reject(message) {
    print FILENAME ":" FNR ": " message > "/dev/stderr"
    failed = 1
    exit 1
}
function quoted(text) {
    if (text == "" || text ~ /["\\[:cntrl:]]/) reject("invalid fixture text")
    return "\"" text "\""
}
function whole(text) { return text ~ /^(0|[1-9][0-9]*)$/ }
function row_prefix() { return rows++ ? "  , " : "    " }
FNR == 1 {
    if (NR == 1) {
        if ($0 != "store\tsku\tdepartment\taffinity\tproduct\tsale_unit\tcents\tstock\tfront_from\tfront_through\tstaple\tingredient") reject("catalog header")
    } else {
        if (!rows || $0 != "store\tsku\tmonths_ago\tquantity") reject("purchase header")
        print "  ]\n\nexport\nsample_purchases : List PastPurchase\nsample_purchases = ["
        purchases = 1
        rows = 0
    }
    next
}
{
    if (($1 != "hardware" && $1 != "indian") || $2 !~ /^[0-9]+$/) reject("invalid store or SKU")
    key = $1 SUBSEP $2
    if (!purchases) {
        if (NF != 12 || seen[key]++ || !whole($7) || ($8 != "?" && !whole($8))) reject("invalid sale row")
        if (!whole($9) || !whole($10) || $9+0 > 12 || $10+0 > 12 || (($9 == "0") != ($10 == "0"))) reject("invalid month range")
        if ($11 !~ /^[01]$/) reject("invalid staple flag")
        stock = $8 == "?" ? "Nothing" : "(Just " $8 ")"
        print row_prefix() "StockItem " quoted($1) " " quoted($2) " " quoted($3) " " quoted($4) " " quoted($5) " " quoted($6) " " $7 " " stock " " $9 " " $10 " " ($11 == 1 ? "True" : "False") " " quoted($12)
    } else {
        if (NF != 4 || !(key in seen) || !whole($3) || !whole($4) || !$4) reject("invalid purchase")
        print row_prefix() "Bought " quoted($1) " " quoted($2) " " $3 " " $4
    }
}
END {
    if (failed) exit 1
    if (!purchases) { print "missing purchases table" > "/dev/stderr"; exit 1 }
    print "  ]"
}
