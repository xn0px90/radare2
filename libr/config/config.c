/* radare - LGPL - Copyright 2006-2016 - pancake */

#include "r_config.h"
#include "r_util.h" // r_str_hash, r_str_chop, ...

R_API RConfigNode* r_config_node_new(const char *name, const char *value) {
	RConfigNode *node;
	if (!name || !*name) {
		return NULL;
	}
	node = R_NEW0 (RConfigNode);
	if (!node) {
		return NULL;
	}
	node->name = strdup (name);
	node->hash = r_str_hash (name);
	node->value = strdup (value? value: "");
	node->flags = CN_RW | CN_STR;
	node->i_value = r_num_get (NULL, value);
	return node;
}

R_API RConfigNode* r_config_node_clone(RConfigNode *n) {
	RConfigNode *cn = R_NEW0 (RConfigNode);
	if (!cn) {
		return NULL;
	}
	cn->name = strdup (n->name);
	cn->desc = n->desc? strdup (n->desc): NULL;
	cn->hash = n->hash;
	cn->value = strdup (n->value? n->value: "");
	cn->i_value = n->i_value;
	cn->flags = n->flags;
	cn->setter = n->setter;
	return cn;
}

R_API void r_config_node_free(void *n) {
	RConfigNode *node = (RConfigNode *) n;
	if (!node) {
		return;
	}
	free (node->name);
	free (node->desc);
	free (node->value);
	free (node);
}

R_API void r_config_list(RConfig *cfg, const char *str, int rad) {
	RConfigNode *node;
	RListIter *iter;
	const char *sfx = "";
	const char *pfx = "";
	int len = 0;

	if (!STRNULL (str)) {
		str = r_str_chop_ro (str);
		len = strlen (str);
	}
	switch (rad) {
	case 1:
		pfx = "\"e ";
		sfx = "\"";
	/* fallthrou */
	case 0:
		r_list_foreach (cfg->nodes, iter, node) {
			if (!str || (str && (!strncmp (str, node->name, len)))) {
				cfg->cb_printf ("%s%s = %s%s\n", pfx,
					node->name, node->value, sfx);
			}
		}
		break;
	case 2:
		r_list_foreach (cfg->nodes, iter, node) {
			if (!str || (str && (!strncmp (str, node->name, len)))) {
				if (!str || !strncmp (str, node->name, len)) {
					cfg->cb_printf ("%20s: %s\n", node->name,
						node->desc? node->desc: "");
				}
			}
		}
		break;
	case 'q':
		r_list_foreach (cfg->nodes, iter, node) {
			cfg->cb_printf ("%s\n", node->name);
		}
		break;
	case 'j':
		cfg->cb_printf ("{");
		r_list_foreach (cfg->nodes, iter, node) {
			if (!str || (str && (!strncmp (str, node->name, len)))) {
				if (!str || !strncmp (str, node->name, len)) {
					const char *val = node->value;
					if (node->flags & CN_BOOL || node->flags & CN_INT || node->flags & CN_OFFT) {
						if (!val) {
							val = "0";
						}
						if (!strncmp (val, "0x", 2)) {
							ut64 n = r_num_get (NULL, val);
							cfg->cb_printf ("\"%s\":%"PFMT64d,
								node->name, n);
						} else if (r_str_isnumber (val) || !strcmp (val, "true") || !strcmp (val, "false")) {
							cfg->cb_printf ("\"%s\":%s",
								node->name, val);
						} else {
							char *sval = r_str_escape (val);
							cfg->cb_printf ("\"%s\":\"%s\"", node->name, sval);
							free (sval);
						}
					} else {
						cfg->cb_printf ("\"%s\":\"%s\"",
							node->name, val);
					}
					if (iter->n) {
						cfg->cb_printf (",");
					}
				}
			}
		}
		cfg->cb_printf ("}\n");
		break;
	}
}

R_API RConfigNode* r_config_node_get(RConfig *cfg, const char *name) {
	if (STRNULL (name)) {
		return NULL;
	}
	return r_hashtable_lookup (cfg->ht, r_str_hash (name));
}

R_API int r_config_set_getter(RConfig *cfg, const char *key, RConfigCallback cb) {
	RConfigNode *node = r_config_node_get (cfg, key);
	if (node) {
		node->getter = cb;
		return 1;
	}
	return 0;
}

R_API int r_config_set_setter(RConfig *cfg, const char *key, RConfigCallback cb) {
	RConfigNode *node = r_config_node_get (cfg, key);
	if (node) {
		node->setter = cb;
		return 1;
	}
	return 0;
}

static bool is_true(const char *s) {
	return !strcasecmp ("true", s) || !strcasecmp ("1", s);
}

static bool is_bool(const char *s) {
	return !strcasecmp ("true", s) || !strcasecmp ("false", s);
}

R_API const char* r_config_get(RConfig *cfg, const char *name) {
	RConfigNode *node = r_config_node_get (cfg, name);
	if (node) {
		if (node->getter) {
			node->getter (cfg->user, node);
		}
		cfg->last_notfound = 0;
		if (node->flags & CN_BOOL) {
			return r_str_bool (is_true (node->value));
		}
		return node->value;
	} else {
		eprintf ("r_config_get: variable '%s' not found\n", name);
	}
	cfg->last_notfound = 1;
	return NULL;
}

R_API int r_config_toggle(RConfig *cfg, const char *name) {
	RConfigNode *node = r_config_node_get (cfg, name);
	if (node && node->flags & CN_BOOL) {
		r_config_set_i (cfg, name, !node->i_value);
		return true;
	}
	return false;
}

R_API ut64 r_config_get_i(RConfig *cfg, const char *name) {
	RConfigNode *node = r_config_node_get (cfg, name);
	if (node) {
		if (node->getter) {
			node->getter (cfg->user, node);
		}
		if (node->i_value || !strcmp (node->value, "false")) {
			return node->i_value;
		}
		return (ut64) r_num_math (cfg->num, node->value);
	}
	return (ut64) 0LL;
}

R_API const char* r_config_node_type(RConfigNode *node) {
	if (node) {
		int f = node->flags;
		if (f & CN_BOOL) {
			return "bool";
		}
		if (f & CN_STR) {
			return "str";
		}
		if (f & CN_OFFT || f & CN_INT) {
			if (!strncmp (node->value, "0x", 2)) {
				return "addr";
			}
			return "int";
		}
	}
	return "";
}

R_API RConfigNode* r_config_set_cb(RConfig *cfg, const char *name, const char *value, RConfigCallback cb) {
	RConfigNode *node = r_config_set (cfg, name, value);
	if (node && (node->setter = cb)) {
		if (!cb (cfg->user, node)) {
			return NULL;
		}
	}
	return node;
}

R_API RConfigNode* r_config_set_i_cb(RConfig *cfg, const char *name, int ivalue, RConfigCallback cb) {
	RConfigNode *node = r_config_set_i (cfg, name, ivalue);
	if (node && (node->setter = cb)) {
		if (!node->setter (cfg->user, node)) {
			return NULL;
		}
	}
	return node;
}

/* TODO: reduce number of strdups here */
R_API RConfigNode* r_config_set(RConfig *cfg, const char *name, const char *value) {
	RConfigNode *node;
	char *ov = NULL;
	ut64 oi;
	if (!cfg || STRNULL (name)) {
		return NULL;
	}
	node = r_config_node_get (cfg, name);
	if (node) {
		if (node->flags & CN_RO) {
			eprintf ("(error: '%s' config key is read only)\n", name);
			return node;
		}
		oi = node->i_value;
		if (node->value) {
			ov = strdup (node->value);
		} else {
			free (node->value);
			node->value = strdup ("");
		}
		if (node->flags & CN_BOOL) {
			bool b = is_true (value);
			node->i_value = (ut64) b? 1: 0;
			free (node->value);
			node->value = strdup (r_str_bool (b));
		} else {
			if (!value) {
				free (node->value);
				node->value = strdup ("");
				node->i_value = 0;
			} else {
				free (node->value);
				node->value = strdup (value);
				if (IS_DIGIT (*value)) {
					if (strchr (value, '/')) {
						node->i_value = r_num_get (cfg->num, value);
					} else {
						node->i_value = r_num_math (cfg->num, value);
					}
				} else {
					node->i_value = 0;
				}
				node->flags |= CN_INT;
			}
		}
	} else { // Create a new RConfigNode
		oi = UT64_MAX;
		if (!cfg->lock) {
			node = r_config_node_new (name, value);
			if (node) {
				if (value && is_bool (value)) {
					node->flags |= CN_BOOL;
					node->i_value = is_true (value)? 1: 0;
				}
				if (cfg->ht) {
					r_hashtable_insert (cfg->ht, node->hash, node);
					r_list_append (cfg->nodes, node);
					cfg->n_nodes++;
				}
			} else {
				eprintf ("r_config_set: unable to create a new RConfigNode\n");
			}
		} else {
			eprintf ("r_config_set: variable '%s' not found\n", name);
		}
	}

	if (node && node->setter) {
		int ret = node->setter (cfg->user, node);
		if (ret == false) {
			if (oi != UT64_MAX) {
				node->i_value = oi;
			}
			free (node->value);
			node->value = strdup (ov? ov: "");
			free (ov);
			return NULL;
		}
	}
	free (ov);
	return node;
}

/* r_config_desc takes a RConfig and a name,
 * r_config_node_desc takes a RConfigNode
 * Both set and return node->desc */
R_API const char* r_config_desc(RConfig *cfg, const char *name, const char *desc) {
	RConfigNode *node = r_config_node_get (cfg, name);
	return r_config_node_desc (node, desc);
}

R_API const char* r_config_node_desc(RConfigNode *node, const char *desc) {
	if (node) {
		if (desc) {
			free (node->desc);
			node->desc = strdup (desc);
		}
		return node->desc;
	}
	return NULL;
}

R_API int r_config_rm(RConfig *cfg, const char *name) {
	RConfigNode *node = r_config_node_get (cfg, name);
	if (node) {
		r_hashtable_remove (cfg->ht, node->hash);
		r_list_delete_data (cfg->nodes, node);
		cfg->n_nodes--;
		return true;
	}
	return false;
}

R_API RConfigNode* r_config_set_i(RConfig *cfg, const char *name, const ut64 i) {
	char buf[128], *ov = NULL;
	RConfigNode *node;
	if (!cfg || !name) {
		return NULL;
	}

	node = r_config_node_get (cfg, name);
	if (node) {
		if (node->flags & CN_RO) {
			return NULL;
		}
		if (node->value) {
			ov = strdup (node->value);
			free (node->value);
		}
		if (node->flags & CN_BOOL) {
			node->value = strdup (r_str_bool (i));
		} else {
			snprintf (buf, sizeof (buf) - 1, "%" PFMT64d, i);
			node->value = strdup (buf);
		}
		//node->flags = CN_RW | CN_INT;
		node->i_value = i;
	} else {
		if (!cfg->lock) {
			if (i < 1024) {
				snprintf (buf, sizeof (buf), "%" PFMT64d "", i);
			} else {
				snprintf (buf, sizeof (buf), "0x%08" PFMT64x "", i);
			}
			node = r_config_node_new (name, buf);
			if (!node) {
				return NULL;
			}
			node->flags = CN_RW | CN_OFFT;
			node->i_value = i;
			if (cfg->ht) {
				r_hashtable_insert (cfg->ht, node->hash, node);
			}
			if (cfg->nodes) {
				r_list_append (cfg->nodes, node);
				cfg->n_nodes++;
			}
		} else {
			eprintf ("(locked: no new keys can be created (%s))\n", name);
		}
	}

	if (node && node->setter) {
		ut64 oi = node->i_value;
		int ret = node->setter (cfg->user, node);
		if (!ret) {
			node->i_value = oi;
			free (node->value);
			node->value = strdup (ov? ov: "");
		}
	}
	free (ov);
	return node;
}

R_API int r_config_eval(RConfig *cfg, const char *str) {
	char *ptr, *a, *b, name[1024];
	unsigned int len;
	if (!str || !cfg) {
		return false;
	}
	len = strlen (str) + 1;
	if (len >= sizeof (name)) {
		return false;
	}
	memcpy (name, str, len);
	str = r_str_chop (name);

	if (!str) {
		return false;
	}

	if (str[0] == '\0' || !strcmp (str, "help")) {
		r_config_list (cfg, NULL, 0);
		return false;
	}

	if (str[0] == '-') {
		r_config_rm (cfg, str + 1);
		return false;
	}

	ptr = strchr (str, '=');
	if (ptr) {
		/* set */
		ptr[0] = '\0';
		a = r_str_chop (name);
		b = r_str_chop (ptr + 1);
		r_config_set (cfg, a, b);
	} else {
		char *foo = r_str_chop (name);
		if (foo[strlen (foo) - 1] == '.') {
			r_config_list (cfg, name, 0);
			return false;
		} else {
			/* get */
			const char *str = r_config_get (cfg, foo);
			if (str) {
				cfg->cb_printf ("%s\n",
					(((int) (size_t) str) == 1)? "true": str);
			}
		}
	}
	return true;
}

static int cmp(RConfigNode *a, RConfigNode *b) {
	return strcmp (a->name, b->name);
}

R_API void r_config_lock(RConfig *cfg, int l) {
	r_list_sort (cfg->nodes, (RListComparator) cmp);
	cfg->lock = l;
}

R_API int r_config_readonly(RConfig *cfg, const char *key) {
	RConfigNode *n = r_config_node_get (cfg, key);
	if (!n) {
		return false;
	}
	n->flags |= CN_RO;
	return true;
}

R_API RConfig* r_config_new(void *user) {
	RConfig *cfg = R_NEW0 (RConfig);
	if (!cfg) {
		return NULL;
	}
	cfg->ht = r_hashtable_new ();
	cfg->nodes = r_list_new ();
	if (!cfg->nodes) {
		R_FREE (cfg);
		return NULL;
	}
	cfg->nodes->free = r_config_node_free;
	cfg->user = user;
	cfg->num = NULL;
	cfg->n_nodes = 0;
	cfg->lock = 0;
	cfg->cb_printf = (void *) printf;
	return cfg;
}

R_API RConfig* r_config_clone(RConfig *cfg) {
	RListIter *iter;
	RConfigNode *node;
	RConfig *c = r_config_new (cfg->user);
	if (!c) {
		return NULL;
	}
	r_list_foreach (cfg->nodes, iter, node) {
		RConfigNode *nn = r_config_node_clone (node);
		r_hashtable_insert (c->ht, node->hash, nn);
		r_list_append (c->nodes, nn);
		c->n_nodes++;
	}
	c->lock = cfg->lock;
	c->cb_printf = cfg->cb_printf;
	return c;
}

R_API int r_config_free(RConfig *cfg) {
	if (!cfg) {
		return 0;
	}
	cfg->nodes->free = r_config_node_free; // damn
	r_list_free (cfg->nodes);
	r_hashtable_free (cfg->ht);
	free (cfg);
	return 0;
}

R_API void r_config_visual_hit_i(RConfig *cfg, const char *name, int delta) {
	RConfigNode *node = r_config_node_get (cfg, name);
	if (node && (node->flags & CN_INT || node->flags & CN_OFFT)) {
		r_config_set_i (cfg, name, r_config_get_i (cfg, name) + delta);
	}
}

R_API void r_config_bump(RConfig *cfg, const char *key) {
	char *orig = strdup (r_config_get (cfg, key));
	r_config_set (cfg, key, orig);
	free (orig);
}

R_API bool r_config_save_char(RConfigHold *h, ...) {
	va_list ap;
	char *key;
	if (!h->list_char) {
		h->list_char = r_list_newf ((RListFree) free);
		if (!h->list_char) {
			return false;
		}
	}
	va_start (ap, h);
	while ((key = va_arg (ap, char *))) {
		RConfigHoldChar *hc = R_NEW0 (RConfigHoldChar);
		if (!hc) {
			continue;
		}
		hc->key = key;
		hc->value = r_config_get (h->cfg, key);
		r_list_append (h->list_char, hc);
	}
	va_end (ap);
	return true;
}

R_API bool r_config_save_num(RConfigHold *h, ...) {
	va_list ap;
	char *key;
	if (!h->list_num) {
		h->list_num = r_list_newf ((RListFree) free);
		if (!h->list_num) {
			return false;
		}
	}
	va_start (ap, h);
	while ((key = va_arg (ap, char *))) {
		RConfigHoldNum *hc = R_NEW0 (RConfigHoldNum);
		if (!hc) {
			continue;
		}
		hc->key = key;
		hc->value = r_config_get_i (h->cfg, key);
		r_list_append (h->list_num, hc);
	}
	va_end (ap);
	return true;
}

R_API RConfigHold* r_config_hold_new(RConfig *cfg) {
	if (cfg) {
		RConfigHold *hold = R_NEW0 (RConfigHold);
		if (hold) {
			hold->cfg = cfg;
			return hold;
		}
	}
	return NULL;
}

R_API void r_config_restore(RConfigHold *h) {
	RListIter *iter;
	RConfigHoldChar *hchar;
	RConfigHoldNum *hnum;
	if (h) {
		r_list_foreach (h->list_num, iter, hnum) {
			r_config_set_i (h->cfg, hnum->key, hnum->value);
		}
		r_list_foreach (h->list_char, iter, hchar) {
			r_config_set (h->cfg, hchar->key, hchar->value);
		}
	}
}

R_API void r_config_hold_free(RConfigHold *h) {
	if (h) {
		r_list_free (h->list_num);
		r_list_free (h->list_char);
		R_FREE (h);
	}
}
