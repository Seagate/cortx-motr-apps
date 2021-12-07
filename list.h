/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 *
 * Original author:  Ganesan Umanesan <ganesan.umanesan@seagate.com>
 * Original creation date: 06-Dec-2021
 */

#ifndef LIST_H_
#define LIST_H_

struct list {
	void *data;
	int size;
	struct list *next;
};

int lsize(struct list **head);
void linit(struct list **head);
void pop(struct list **head, void *data);
void push(struct list **head, void *data, int size);
void lfree(struct list **head);
void list_print_int(struct list *head);
void list_print_str(struct list *head);

#endif /* LIST_H_ */
