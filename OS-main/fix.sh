#!/bin/bash
# Direct fix for tg3.c array bounds issue
# This script modifies the file in-place

FILE="drivers/net/ethernet/broadcom/tg3.c"

if [ ! -f "$FILE" ]; then
    echo "ERROR: File not found: $FILE"
    echo "Run this script from your kernel source root directory"
    exit 1
fi

echo "Backing up $FILE to ${FILE}.backup"
cp "$FILE" "${FILE}.backup"

echo "Applying fixes..."

# Fix all instances of: for (i = 0; i < tp->irq_max; i++)
sed -i 's/for (i = 0; i < tp->irq_max; i++)/for (i = 0; i < tp->irq_max \&\& i < TG3_IRQ_MAX_VECS; i++)/g' "$FILE"

# Fix: for (i = 1; i < tp->irq_max; i++)
sed -i 's/for (i = 1; i < tp->irq_max; i++)/for (i = 1; i < tp->irq_max \&\& i < TG3_IRQ_MAX_VECS; i++)/g' "$FILE"

# Fix: for (; i < tp->irq_max; i++, ...)
sed -i 's/for (; i < tp->irq_max; i++,/for (; i < tp->irq_max \&\& i < TG3_IRQ_MAX_VECS; i++,/g' "$FILE"

# Fix: for (; i < tp->irq_max; i++)
sed -i 's/for (; i < tp->irq_max; i++)/for (; i < tp->irq_max \&\& i < TG3_IRQ_MAX_VECS; i++)/g' "$FILE"
sed -i 's/xt_TCPMSS/xt_tcpmss/g' net/netfilter/Makefile

echo "Done!"
echo ""
echo "Verifying line 17794:"
sed -n '17794p' "$FILE"
echo ""
echo "If the above line contains '&& i < TG3_IRQ_MAX_VECS', the fix was successful."
echo ""
echo "Now recompile with:"
echo "  make drivers/net/ethernet/broadcom/tg3.o"