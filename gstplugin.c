/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2017 Siwon Kang <<kkangshawn@gmail.com>>
 * Copyright (C) 2023 Sebastian Ovelar <<sebastianrovelar@gmail.com>>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-gzdec
 *
 * gzip decoder that receives a stream compressed with gzip and emits an
 * uncompressed stream.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m filesrc location=file.txt.gz ! gzdec ! filesink location="file.txt"
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstgzdec.h"



GST_DEBUG_CATEGORY_STATIC (gst_gzdec_debug);
#define GST_CAT_DEFAULT gst_gzdec_debug

/* Filter signals and args */
enum
{
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT
};

/* the capabilities of the inputs and outputs.
 * filesrc and filesink also set its capability to ANY so leave both as ANY.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
);

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

#define gst_gzdec_parent_class parent_class
G_DEFINE_TYPE (Gstgzdec, gst_gzdec, GST_TYPE_ELEMENT);

static void gst_gzdec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gzdec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_gzdec_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_gzdec_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);

/* GObject vmethod implementations */

/* initialize the gzdec's class */
static void
gst_gzdec_class_init (GstgzdecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_gzdec_set_property;
  gobject_class->get_property = gst_gzdec_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  gst_element_class_set_details_simple(gstelement_class,
    "gzip decoder",
    "Decoder/File",
    "Receives a stream compressed with gzip and emits an uncompressed stream",
    "Siwon Kang <<kkangshawn@gmail.com>>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_gzdec_init (Gstgzdec * filter)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_gzdec_sink_event));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_gzdec_chain));
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->silent = FALSE;

  init_decoder ();
}

static void
gst_gzdec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstgzdec *filter = GST_GZDEC (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gzdec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstgzdec *filter = GST_GZDEC (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_gzdec_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  Gstgzdec *filter;
  gboolean ret;

  filter = GST_GZDEC (parent);

  GST_LOG_OBJECT (filter, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps * caps;

      gst_event_parse_caps (event, &caps);
      /* do something with the caps */

      /* and forward */
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}
z_stream stream;

gint
init_decoder (void)
{
  gint ret;

  /* allocate inflate state */
  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  stream.opaque = Z_NULL;
  stream.avail_in = 0;
  stream.next_in = Z_NULL;

  ret = inflateInit2 (&stream, 16 + MAX_WBITS);

  return ret;
}

void
deinit_decoder (void)
{
  /* clean up and return */
  (void)inflateEnd(&stream);
}

gint
decode_message (const guchar * srcmsg, const gint srclen, guchar ** outmsg, gulong * outlen)
{
  gint ret;
  guchar inbuffer[CHUNK];
  guchar outbuffer[CHUNK];
  gulong processed;
  struct ll_buffer *outdata, *head;
  guchar *srcidx = (guchar *)srcmsg;
  gint remainder = srclen;

  outdata = g_malloc0 (sizeof(struct ll_buffer));
  head = outdata;

  do {
    memset (inbuffer, 0, CHUNK);
    if (remainder >= CHUNK) {
        remainder -= CHUNK;
        stream.avail_in = CHUNK;
        memcpy (inbuffer, srcidx, CHUNK);
        srcidx += CHUNK;
    }
    else {
        stream.avail_in = remainder;
        memcpy (inbuffer, srcidx, remainder);
        remainder = 0;
    }

    if (stream.avail_in == 0)
      break;
    stream.next_in = inbuffer;

    do {
      outdata->data = g_malloc0 (CHUNK);

      stream.avail_out = CHUNK;
      stream.next_out = outbuffer;
      ret = inflate (&stream, Z_NO_FLUSH);
      switch (ret) {
      case Z_NEED_DICT:
        ret = Z_DATA_ERROR;
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
      case Z_STREAM_ERROR:
        (void)inflateEnd(&stream);
        return ret;
      }
      processed = CHUNK - stream.avail_out;
      memcpy (outdata->data + outdata->len, outbuffer, processed);
      outdata->len += processed;
      *outlen += processed;

      if (stream.avail_out == 0) {
        outdata->next = g_malloc0 (sizeof(struct ll_buffer));
        outdata = outdata->next;
      }

    } while (stream.avail_out == 0);

    if (ret != Z_STREAM_END) {
      outdata->next = g_malloc0 (sizeof(struct ll_buffer));
      outdata = outdata->next;
    }
  } while (ret != Z_STREAM_END);

  outdata = head;
  *outmsg = g_malloc0 (*outlen);

  gulong outmsgidx = 0;

  while (outdata) {
    memcpy (*outmsg + outmsgidx, outdata->data, outdata->len);

    outmsgidx += outdata->len;
    g_free (outdata->data);

    struct ll_buffer *temp = outdata;
    outdata = outdata->next;
    g_free (temp);
  }

  return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}
static GstBuffer *
gst_gzdec_process_data (GstBuffer * buf)
{
  GstBuffer *outbuf = NULL;
  GstMapInfo info;
  GstMemory *mem = NULL;
  guchar *srcmsg, *decodedmsg = NULL;
  gulong decodedmsglen = 0;

  gst_buffer_map (buf, &info, GST_MAP_READ);
  srcmsg = (guchar *)info.data;

  g_print ("Source message: %s\n", srcmsg);
  decode_message (srcmsg, gst_buffer_get_size(buf), &decodedmsg, &decodedmsglen);

  g_print ("Decoded message: %s(%lu)\n", decodedmsg, decodedmsglen);
  outbuf = gst_buffer_new ();
  mem = gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
                  decodedmsg, decodedmsglen, 0, decodedmsglen, NULL, NULL);
  gst_buffer_append_memory (outbuf, mem);

  memset (info.data, 0xff, info.size);
  gst_buffer_unmap (buf, &info);

  return outbuf;
}
/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_gzdec_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  Gstgzdec *filter;
  GstBuffer *outbuf;

  filter = GST_GZDEC (parent);

  if (filter->silent == FALSE)
    g_print ("Have data of size %" G_GSIZE_FORMAT" bytes!\n",
                    gst_buffer_get_size(buf));
  outbuf = gst_gzdec_process_data (buf);
  gst_buffer_unref (buf);
  if (!outbuf) {
    GST_ELEMENT_ERROR (GST_ELEMENT (filter), STREAM, FAILED, (NULL), (NULL));
    return GST_FLOW_ERROR;
  }
  /* push out the output buffer to source pad after processing it */
  return gst_pad_push (filter->srcpad, outbuf);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
gzdec_init (GstPlugin * gzdec)
{
  /* debug category for filtering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_gzdec_debug, "gzdec",
      0, "gzip decoder plugin");

  return gst_element_register (gzdec, "gzdec", GST_RANK_NONE,
      GST_TYPE_GZDEC);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstgzdec"
#endif

/* gstreamer looks for this structure to register gzdecs */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    gzdec,
    "gzip decoder plugin",
    gzdec_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
