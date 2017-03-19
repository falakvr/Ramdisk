/*
 * ramdisk.c
 *
 *  Created on: Nov 24, 2016
 *      Author: falak
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define debug 0
#define SIZE 4096

typedef struct tree_node {
	char name[256];
	char type;
	char *contents;
	struct tree_node *parent;
	struct tree_node *child;
	struct tree_node *next;
	struct stat *st;
} Node;

Node *root;
long int fs_mem;
char newFile[256];

void ramdisk_init();

int pathType(const char *path) {

	char *token;

	char dupPath[SIZE];
	strcpy(dupPath, path);

	token = strtok(dupPath, "/");

	//Check if it is  root

	if (token == NULL && strcmp(path, "/") == 0) {

		return 0;

	} else {

		int childFlag = 0;
		Node *temp = root;
		Node *tempChild = NULL;

		while (token != NULL) {

			tempChild = temp->child;
			//Check all child nodes

			while (tempChild) {

				if (strcmp(tempChild->name, token) == 0) {

					if (debug)
						printf("Found a child. Set flag\n");

					childFlag = 1;
					break;
				}
				//Look for next child in the linked list
				tempChild = tempChild->next;
			}

			token = strtok(NULL, "/");

			if (childFlag == 1) {

				if (token == NULL)
					return 0;
			} else {

				if (token)
					return -1; //valid path
				else
					return 1; // invalid path
			}
			//One level down

			temp = tempChild;
			childFlag = 0;
		}
	}
	return -1;
}

Node *findPathNode(const char *path) {
	if (debug)
		printf("Inside findPathNode\n");

	char *token;
	char dupPath[SIZE];
	strcpy(dupPath, path);

	token = strtok(dupPath, "/");

	//Check if it is  root

	if (token == NULL && strcmp(path, "/") == 0) {

		return root;

	} else {

		int childFlag = 0;
		Node *temp = root;
		Node *tempChild = NULL;

		while (token != NULL) {

			tempChild = temp->child;
			//Check all child nodes

			while (tempChild) {

				if (strcmp(tempChild->name, token) == 0) {

					childFlag = 1;
					break;
				}

				//Look for next child in the linked list

				tempChild = tempChild->next;
			}

			if (childFlag == 1) {

				strcpy(newFile, token);

				token = strtok(NULL, "/");

				if (token == NULL) {

					if (tempChild == NULL)
						return temp;
					else
						return tempChild;
				}

			} else {

				strcpy(newFile, token);

				return temp;
			}
			temp = tempChild;
		}
	}
	return NULL;
}

static int ramdisk_getattr(const char *path, struct stat *st) {
	Node *node;

	int ret = 0;
	int type = pathType(path);

	if (type == 0) {
		node = findPathNode(path);

		st->st_uid = node->st->st_uid;
		st->st_gid = node->st->st_gid;
		st->st_atime = node->st->st_atime;
		st->st_mtime = node->st->st_mtime;
		st->st_ctime = node->st->st_ctime;
		st->st_nlink = node->st->st_nlink;
		st->st_size = node->st->st_size;
		st->st_mode = node->st->st_mode;

	} else {
		ret = -ENOENT;
	}

	return ret;
}

static int ramdisk_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info* fi) {

	(void) offset;
	(void) fi;

	int ret = 0;

	Node *node;
	Node *childNode = NULL;

	int type = pathType(path);

	if ((type == 0) || (type == 1)) {

		node = findPathNode(path);

		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

		childNode = node->child;

		while (childNode != NULL) {
			filler(buf, childNode->name, NULL, 0);
			childNode = childNode->next;
			if (debug)
				printf("Next child node = %s\n", childNode->name);
		}

		node->st->st_atime = time(NULL);

		ret = 0;
	} else
		ret = -ENOENT;

	return ret;
}

static int ramdisk_opendir(const char* path, struct fuse_file_info* fi) {
	return 0;
}

static int ramdisk_open(const char* path, struct fuse_file_info* fi) {

	int ret = 0;

	int type = pathType(path);

	if (type == 0) {
		ret = 0;
	} else
		ret = -ENOENT;

	return ret;
}

static int ramdisk_mkdir(const char* path, mode_t mode) {

	Node *node = findPathNode(path);

	Node *temp;

	temp = (Node *) malloc(sizeof(Node));
	temp->st = (struct stat *) malloc(sizeof(struct stat));

	if (temp == NULL) {
		return -ENOSPC;
	}

	long int node_size = sizeof(Node) + sizeof(stat);
	fs_mem -= node_size;

	if (fs_mem < 0) {
		return -ENOSPC;
	}

	strcpy(temp->name, newFile);

	temp->type = 'd';

	temp->parent = node;

	temp->next = NULL;
	temp->child = NULL;

	temp->st->st_uid = getuid();
	temp->st->st_mode = S_IFDIR | 0755;
	temp->st->st_gid = getgid();

	temp->st->st_nlink = 2;
	temp->st->st_atime = time(NULL);
	temp->st->st_mtime = time(NULL);
	temp->st->st_ctime = time(NULL);

	temp->st->st_size = SIZE;

	Node *childNode;
	childNode = node->child;
	if (childNode != NULL) {
		while (childNode->next != NULL) {
			childNode = childNode->next;
		}
		childNode->next = temp;
	} else {
		node->child = temp;
	}

	node->st->st_nlink += 1;

	return 0;
}

static int ramdisk_create(const char *path, mode_t mode,
		struct fuse_file_info *fi) {

	if (fs_mem < 0) {
		return -ENOSPC;
	}

	Node *node = findPathNode(path);

	Node *temp;

	temp = (Node *) malloc(sizeof(Node));
	temp->st = (struct stat *) malloc(sizeof(struct stat));

	if (temp == NULL) {
		return -ENOSPC;
	}

	long int node_size = sizeof(Node) + sizeof(stat);
	fs_mem -= node_size;

	strcpy(temp->name, newFile);

	temp->type = 'f';

	temp->parent = node;

	if (debug)
		printf("Parent = %s\n", node->name);

	temp->next = NULL;
	temp->child = NULL;

	temp->st->st_uid = getuid();
	temp->st->st_mode = S_IFREG | mode;
	temp->st->st_gid = getgid();

	temp->st->st_nlink = 1;
	temp->st->st_atime = time(NULL);
	temp->st->st_mtime = time(NULL);
	temp->st->st_ctime = time(NULL);

	temp->st->st_size = 0;
	temp->contents = NULL;

	Node *childNode;
	childNode = node->child;
	if (childNode != NULL) {

		while (childNode->next != NULL) {
			childNode = childNode->next;
		}

		childNode->next = temp;

	} else {

		node->child = temp;
	}

	return 0;

}

static int ramdisk_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi) {

	Node *node;
	size_t fileSize;

	node = findPathNode(path);

	if (node->type == 'd') {
		return -EISDIR;
	}

	fileSize = node->st->st_size;

	if (debug)
		printf("Size of the file is = %zu\n", fileSize);

	if (fileSize < offset) {
		size = 0;
	} else if (offset < fileSize) {
		if (offset + size > fileSize) {
			size = fileSize - offset;
		}
		memcpy(buf, node->contents + offset, size);
	}

	if (debug)
		printf("Reading %zu bytes\n", size);

	if (size > 0) {
		node->st->st_atime = time(NULL);
	}

	return size;
}

static int ramdisk_rmdir(const char *path) {

	int type;
	Node *node;
	Node *parent;
	Node *childNode;

	type = pathType(path);

	if (debug)
		printf("Type = %d\n", type);

	if (type == 0) {

		node = findPathNode(path);

		if (node->child != NULL) {

			return -ENOTEMPTY;

		} else {

			parent = node->parent;

			if (parent->child == node) {

				if (node->next == NULL) {

					//Node to be deleted is only child of the parent

					parent->child = NULL;

				} else {

					//Not the only child

					parent->child = node->next;
				}
			} else {

				//Find intermediate node in the linked list

				for (childNode = parent->child; childNode != NULL; childNode =
						childNode->next) {
					if (childNode->next == node) {
						childNode->next = node->next;
						break;
					}
				}
			}

			free(node->st);
			free(node);
			fs_mem += sizeof(Node) + sizeof(struct stat);

			parent->st->st_nlink -= 1;
			return 0;
		}
	} else
		return -ENOENT;
}

static int ramdisk_unlink(const char *path) {

	if (debug)
		printf("Inside unlink\n");

	int type;
	Node *node;
	Node *parent;
	Node *childNode;

	type = pathType(path);

	if (type != 0) {
		return -ENOENT;
	}

	node = findPathNode(path);

	parent = node->parent;

	if (parent->child == node) {

		if (node->next == NULL) {

			//Node to be deleted is only child of the parent

			parent->child = NULL;

		} else {

			//Not the only child

			parent->child = node->next;
		}
	} else {

		//Find intermediate node in the linked list

		for (childNode = parent->child; childNode != NULL;
				childNode = childNode->next) {
			if (childNode->next == node) {
				childNode->next = node->next;
				break;
			}
		}
	}

	if (node->st->st_size > 0) {

		fs_mem += node->st->st_size;

		free(node->contents);
		free(node->st);
		free(node);

	} else {

		free(node->st);
		free(node);

	}

	fs_mem += sizeof(Node) + sizeof(struct stat);

	return 0;
}

static int ramdisk_write(const char *path, const char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi) {

	size_t fileSize;

	if (size > fs_mem) {
		return -ENOSPC;
	}

	Node *node;
	node = findPathNode(path);

	if (node->type == 'd') {
		return -EISDIR;
	}

	fileSize = node->st->st_size;

	if (size > 0) {

		if (fileSize == 0) {

			offset = 0;

			node->contents = (char *) malloc(sizeof(char) * size);

			memcpy(node->contents + offset, buf, size);

			node->st->st_size = offset + size;

			node->st->st_ctime = time(NULL);
			node->st->st_mtime = time(NULL);

			fs_mem -= size;

		} else {

			if (offset > fileSize) {
				offset = fileSize;

			}

			char *more = (char *) realloc(node->contents,
					sizeof(char) * (offset + size));

			if (more == NULL) {

				return -ENOSPC;

			} else {

				node->contents = more;

				// Change pointer of file contents to a newly reallocated memory pointer

				memcpy(node->contents + offset, buf, size);
				node->st->st_size = offset + size;

				node->st->st_ctime = time(NULL);
				node->st->st_mtime = time(NULL);

				fs_mem -= (offset + size);
			}
		}
	}
	return size;
}

static int ramdisk_truncate(const char *path, off_t size) {

	int type;

	type = pathType(path);

	if (type == 0) {
		return 0;
	} else {
		return -ENOENT;
	}
}

static int ramdisk_utimens(const char* path, const struct timespec ts[2]) {
	int type;

	type = pathType(path);

	if (type == 0) {

		return 0;
	} else {
		return -ENOENT;
	}
}

static struct fuse_operations ramdisk_oper = { .getattr = ramdisk_getattr,
		.readdir = ramdisk_readdir, .open = ramdisk_open, .opendir =
				ramdisk_opendir, .read = ramdisk_read, .write = ramdisk_write,
		.mkdir = ramdisk_mkdir, .rmdir = ramdisk_rmdir,
		.create = ramdisk_create, .truncate = ramdisk_truncate, .unlink =
				ramdisk_unlink, .utimens = ramdisk_utimens, };

int main(int argc, char *argv[]) {

	if (argc != 3) {
		printf("Incorrect usage!\n");
		printf("Correct usage: ./ramdisk <mount_point> <size>\n");

		exit(EXIT_FAILURE);
	}

	fs_mem = atoi(argv[argc - 1]);

	fs_mem = fs_mem * 1024 * 1024;
	ramdisk_init();
	fuse_main(argc - 1, argv, &ramdisk_oper, NULL);
	return 0;
}

void ramdisk_init() {

	root = (Node *) malloc(sizeof(Node));
	root->st = (struct stat *) malloc(sizeof(struct stat));

	root->type = 'd';
	root->parent = NULL;
	strcpy(root->name, "/");
	root->next = NULL;
	root->child = NULL;
	root->st->st_uid = getuid();
	root->st->st_mode = S_IFDIR | 0755;
	root->st->st_gid = getgid();
	root->st->st_nlink = 2;
	root->st->st_atime = time(NULL);
	root->st->st_mtime = time(NULL);
	root->st->st_ctime = time(NULL);
	root->contents = NULL;

	long int root_size = sizeof(Node) + sizeof(stat);

	fs_mem -= root_size;

}
