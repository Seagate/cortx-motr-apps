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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"

void linit(struct list **head)
{
	*head = NULL;
	return;
}

void lfree(struct list **head)
{
	struct list *ptr;
	ptr = *head;
	while(*head != NULL) {
		ptr = *head;
		*head = (*head)->next;
		free(ptr->data);
		free(ptr);
	}
	return;
}

int lsize(struct list **head)
{
	int length=0;
	struct list *ptr;
	ptr = *head;
	while(ptr != NULL) {
		length++;
		ptr = ptr->next;
	}
	return length;
}

void push(struct list **head, void *data, int size)
{
	struct list *tmp;
	struct list *ptr;

	tmp = malloc(sizeof(struct list));
	tmp->data = malloc(size);
	memcpy(tmp->data,data,size);
	tmp->size = size;
	tmp->next = NULL;

	ptr = *head;
	*head = tmp;
	tmp->next = ptr;
	return;
}

void pop(struct list **head, void *data)
{
	if(!(*head)) printf("error! list is empty!!\n");
	struct list *ptr;
	memcpy(data,(*head)->data,(*head)->size);
	ptr = *head;
	*head = (*head)->next;
	free(ptr->data);
	free(ptr);
	return;
}

void list_print_int(struct list *head)
{
	struct list *ptr;
	ptr = head;
	while(ptr!=NULL) {
		printf("%d ", *(int *)ptr->data);
		ptr = ptr->next;
	}
	printf("\n");
	return;
}

void list_print_str(struct list *head)
{
	struct list *ptr;
	ptr = head;
	while(ptr!=NULL) {
		printf("%s\n", (char *)ptr->data);
		ptr = ptr->next;
	}
	return;
}

