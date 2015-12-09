/**
 * at_item.h --- at item implementation for the phoneserver
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */
struct at_item {
    void *adapter; // ## ptr to adapter
    enum mode;
    char tag_str[]; // ## attribute tag_str
};
