/**
 * at_item.h --- at item implementation for the phoneserver
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef AT_ITEM_H_
#define AT_ITEM_H_

struct at_item {
    void *adapter;  // ptr to adapter
    enum mode;
    char tag_str[];  // attribute tag_str
};

#endif  // AT_ITEM_H_
