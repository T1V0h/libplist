/*
 * plist.c
 * XML plist implementation
 *
 * Copyright (c) 2008 Jonathan Beck All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>


#include <libxml/parser.h>
#include <libxml/tree.h>

#include "plist.h"

#define XPLIST_TEXT	BAD_CAST("text")
#define XPLIST_KEY	BAD_CAST("key")
#define XPLIST_FALSE	BAD_CAST("false")
#define XPLIST_TRUE	BAD_CAST("true")
#define XPLIST_INT	BAD_CAST("integer")
#define XPLIST_REAL	BAD_CAST("real")
#define XPLIST_DATE	BAD_CAST("date")
#define XPLIST_DATA	BAD_CAST("data")
#define XPLIST_STRING	BAD_CAST("string")
#define XPLIST_ARRAY	BAD_CAST("array")
#define XPLIST_DICT	BAD_CAST("dict")

static const char *plist_base = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n\
<plist version=\"1.0\">\n\
</plist>\0";


/** Formats a block of text to be a given indentation and width.
 *
 * The total width of the return string will be depth + cols.
 *
 * @param buf The string to format.
 * @param cols The number of text columns for returned block of text.
 * @param depth The number of tabs to indent the returned block of text.
 *
 * @return The formatted string.
 */
static gchar *format_string(const char *buf, int cols, int depth)
{
    int colw = depth + cols + 1;
    int len = strlen(buf);
    int nlines = len / cols + 1;
    gchar *new_buf = (gchar *) g_malloc0(nlines * colw + depth + 1);
    int i = 0;
    int j = 0;

    assert(cols >= 0);
    assert(depth >= 0);

    // Inserts new lines and tabs at appropriate locations
    for (i = 0; i < nlines; i++)
    {
        new_buf[i * colw] = '\n';
        for (j = 0; j < depth; j++)
            new_buf[i * colw + 1 + j] = '\t';
        memcpy(new_buf + i * colw + 1 + depth, buf + i * cols, (i + 1) * cols <= len ? cols : len - i * cols);
    }
    new_buf[len + (1 + depth) * nlines] = '\n';

    // Inserts final row of indentation and termination character
    for (j = 0; j < depth; j++)
        new_buf[len + (1 + depth) * nlines + 1 + j] = '\t';
    new_buf[len + (1 + depth) * nlines + depth + 1] = '\0';

    return new_buf;
}



struct xml_node
{
    xmlNodePtr xml;
    uint32_t depth;
};

/** Creates a new plist XML document.
 *
 * @return The plist XML document.
 */
static xmlDocPtr new_xml_plist(void)
{
    char *plist = strdup(plist_base);
    xmlDocPtr plist_xml = xmlParseMemory(plist, strlen(plist));

    if (!plist_xml)
        return NULL;

    free(plist);

    return plist_xml;
}

static void node_to_xml(GNode * node, gpointer xml_struct)
{
    struct xml_node *xstruct = NULL;
    plist_data_t node_data = NULL;

    xmlNodePtr child_node = NULL;
    char isStruct = FALSE;

    const xmlChar *tag = NULL;
    gchar *val = NULL;

    //for base64
    gchar *valtmp = NULL;

    uint32_t i = 0;

    if (!node)
        return;

    xstruct = (struct xml_node *) xml_struct;
    node_data = plist_get_data(node);

    switch (node_data->type)
    {
    case PLIST_BOOLEAN:
    {
        if (node_data->boolval)
            tag = XPLIST_TRUE;
        else
            tag = XPLIST_FALSE;
    }
    break;

    case PLIST_UINT:
        tag = XPLIST_INT;
        val = g_strdup_printf("%llu", node_data->intval);
        break;

    case PLIST_REAL:
        tag = XPLIST_REAL;
        val = g_strdup_printf("%f", node_data->realval);
        break;

    case PLIST_STRING:
        tag = XPLIST_STRING;
        val = g_strdup(node_data->strval);
        break;

    case PLIST_KEY:
        tag = XPLIST_KEY;
        val = g_strdup((gchar *) node_data->strval);
        break;

    case PLIST_DATA:
        tag = XPLIST_DATA;
        if (node_data->length)
        {
            valtmp = g_base64_encode(node_data->buff, node_data->length);
            val = format_string(valtmp, 60, xstruct->depth);
            g_free(valtmp);
        }
        break;
    case PLIST_ARRAY:
        tag = XPLIST_ARRAY;
        isStruct = TRUE;
        break;
    case PLIST_DICT:
        tag = XPLIST_DICT;
        isStruct = TRUE;
        break;
    case PLIST_DATE:
        tag = XPLIST_DATE;
        val = g_time_val_to_iso8601(&node_data->timeval);
        break;
    default:
        break;
    }

    for (i = 0; i < xstruct->depth; i++)
    {
        xmlNodeAddContent(xstruct->xml, BAD_CAST("\t"));
    }
    if (node_data->type == PLIST_STRING) {
        /* make sure we convert the following predefined xml entities */
        /* < = &lt; > = &gt; ' = &apos; " = &quot; & = &amp; */
        child_node = xmlNewTextChild(xstruct->xml, NULL, tag, BAD_CAST(val));
    } else
        child_node = xmlNewChild(xstruct->xml, NULL, tag, BAD_CAST(val));
    xmlNodeAddContent(xstruct->xml, BAD_CAST("\n"));
    g_free(val);

    //add return for structured types
    if (node_data->type == PLIST_ARRAY || node_data->type == PLIST_DICT)
        xmlNodeAddContent(child_node, BAD_CAST("\n"));

    if (isStruct)
    {
        struct xml_node child = { child_node, xstruct->depth + 1 };
        g_node_children_foreach(node, G_TRAVERSE_ALL, node_to_xml, &child);
    }
    //fix indent for structured types
    if (node_data->type == PLIST_ARRAY || node_data->type == PLIST_DICT)
    {

        for (i = 0; i < xstruct->depth; i++)
        {
            xmlNodeAddContent(child_node, BAD_CAST("\t"));
        }
    }

    return;
}

static void xml_to_node(xmlNodePtr xml_node, plist_t * plist_node)
{
    xmlNodePtr node = NULL;
    plist_data_t data = NULL;
    plist_t subnode = NULL;

    //for string
    glong len = 0;
    int type = 0;

    if (!xml_node)
        return;

    for (node = xml_node->children; node; node = node->next)
    {

        while (node && !xmlStrcmp(node->name, XPLIST_TEXT))
            node = node->next;
        if (!node)
            break;

        data = plist_new_plist_data();
        subnode = plist_new_node(data);
        if (*plist_node)
            g_node_append(*plist_node, subnode);
        else
            *plist_node = subnode;

        if (!xmlStrcmp(node->name, XPLIST_TRUE))
        {
            data->boolval = TRUE;
            data->type = PLIST_BOOLEAN;
            data->length = 1;
            continue;
        }

        if (!xmlStrcmp(node->name, XPLIST_FALSE))
        {
            data->boolval = FALSE;
            data->type = PLIST_BOOLEAN;
            data->length = 1;
            continue;
        }

        if (!xmlStrcmp(node->name, XPLIST_INT))
        {
            xmlChar *strval = xmlNodeGetContent(node);
            data->intval = g_ascii_strtoull((char *) strval, NULL, 0);
            data->type = PLIST_UINT;
            data->length = 8;
            xmlFree(strval);
            continue;
        }

        if (!xmlStrcmp(node->name, XPLIST_REAL))
        {
            xmlChar *strval = xmlNodeGetContent(node);
            data->realval = atof((char *) strval);
            data->type = PLIST_REAL;
            data->length = 8;
            xmlFree(strval);
            continue;
        }

        if (!xmlStrcmp(node->name, XPLIST_DATE))
        {
            xmlChar *strval = xmlNodeGetContent(node);
            g_time_val_from_iso8601((char *) strval, &data->timeval);
            data->type = PLIST_DATE;
            data->length = sizeof(GTimeVal);
            xmlFree(strval);
            continue;
        }

        if (!xmlStrcmp(node->name, XPLIST_STRING))
        {
            xmlChar *strval = xmlNodeGetContent(node);
            len = strlen((char *) strval);
            type = xmlDetectCharEncoding(strval, len);

            if (XML_CHAR_ENCODING_UTF8 == type || XML_CHAR_ENCODING_ASCII == type || XML_CHAR_ENCODING_NONE == type)
            {
                data->strval = strdup((char *) strval);
                data->type = PLIST_STRING;
                data->length = strlen(data->strval);
            }
            xmlFree(strval);
            continue;
        }

        if (!xmlStrcmp(node->name, XPLIST_KEY))
        {
            xmlChar *strval = xmlNodeGetContent(node);
            data->strval = strdup((char *) strval);
            data->type = PLIST_KEY;
            data->length = strlen(data->strval);
            xmlFree(strval);
            continue;
        }

        if (!xmlStrcmp(node->name, XPLIST_DATA))
        {
            xmlChar *strval = xmlNodeGetContent(node);
            gsize size = 0;
            guchar *dec = g_base64_decode((char *) strval, &size);
            data->buff = (uint8_t *) malloc(size * sizeof(uint8_t));
            memcpy(data->buff, dec, size * sizeof(uint8_t));
            g_free(dec);
            data->length = size;
            data->type = PLIST_DATA;
            xmlFree(strval);
            continue;
        }

        if (!xmlStrcmp(node->name, XPLIST_ARRAY))
        {
            data->type = PLIST_ARRAY;
            xml_to_node(node, &subnode);
            continue;
        }

        if (!xmlStrcmp(node->name, XPLIST_DICT))
        {
            data->type = PLIST_DICT;
            xml_to_node(node, &subnode);
            continue;
        }
    }
}

void plist_to_xml(plist_t plist, char **plist_xml, uint32_t * length)
{
    xmlDocPtr plist_doc = NULL;
    xmlNodePtr root_node = NULL;
    struct xml_node root = { NULL, 0 };
    int size = 0;

    if (!plist || !plist_xml || *plist_xml)
        return;
    plist_doc = new_xml_plist();
    root_node = xmlDocGetRootElement(plist_doc);
    root.xml = root_node;

    node_to_xml(plist, &root);

    xmlDocDumpMemory(plist_doc, (xmlChar **) plist_xml, &size);
    if (size >= 0)
        *length = size;
    xmlFreeDoc(plist_doc);
}

void plist_from_xml(const char *plist_xml, uint32_t length, plist_t * plist)
{
    xmlDocPtr plist_doc = xmlParseMemory(plist_xml, length);
    xmlNodePtr root_node = xmlDocGetRootElement(plist_doc);

    xml_to_node(root_node, plist);
    xmlFreeDoc(plist_doc);
}
