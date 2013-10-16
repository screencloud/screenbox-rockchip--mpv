/*
 * This file is part of mplayer.
 *
 * mplayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <sys/stat.h>

#include <libswscale/swscale.h>

#include "config.h"
#include "mpvcore/bstr.h"
#include "osdep/io.h"
#include "mpvcore/path.h"
#include "talloc.h"
#include "mpvcore/mp_msg.h"
#include "video/out/vo.h"
#include "video/csputils.h"
#include "video/vfcap.h"
#include "video/mp_image.h"
#include "video/fmt-conversion.h"
#include "video/image_writer.h"
#include "video/sws_utils.h"
#include "sub/sub.h"
#include "mpvcore/m_option.h"

struct priv {
    struct image_writer_opts *opts;
    char *outdir;

    struct mp_image *current;
    int frame;
};

static bool checked_mkdir(struct vo *vo, const char *buf)
{
    MP_INFO(vo, "Creating output directory '%s'...\n", buf);
    if (mkdir(buf, 0755) < 0) {
        char *errstr = strerror(errno);
        if (errno == EEXIST) {
            struct stat stat_p;
            if (mp_stat(buf, &stat_p ) == 0 && S_ISDIR(stat_p.st_mode))
                return true;
        }
        MP_ERR(vo, "Error creating output directory: %s\n", errstr);
        return false;
    }
    return true;
}

static int reconfig(struct vo *vo, struct mp_image_params *params, int flags)
{
    struct priv *p = vo->priv;
    mp_image_unrefp(&p->current);

    if (p->outdir && vo->config_count < 1)
        if (!checked_mkdir(vo, p->outdir))
            return -1;

    return 0;
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *p = vo->priv;

    mp_image_setrefp(&p->current, mpi);
}

static void draw_osd(struct vo *vo, struct osd_state *osd)
{
    struct priv *p = vo->priv;

    struct aspect_data asp = vo->aspdat;
    double sar = (double)asp.orgw / asp.orgh;
    double dar = (double)asp.prew / asp.preh;

    struct mp_osd_res dim = {
        .w = asp.orgw,
        .h = asp.orgh,
        .display_par = sar / dar,
        .video_par = dar / sar,
    };

    osd_draw_on_image(osd, dim, osd->vo_pts, OSD_DRAW_SUB_ONLY, p->current);
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;

    (p->frame)++;

    void *t = talloc_new(NULL);
    char *filename = talloc_asprintf(t, "%08d.%s", p->frame,
                                     image_writer_file_ext(p->opts));

    if (p->outdir && strlen(p->outdir))
        filename = mp_path_join(t, bstr0(p->outdir), bstr0(filename));

    MP_INFO(vo, "Saving %s\n", filename);
    write_image(p->current, p->opts, filename);

    talloc_free(t);
    mp_image_unrefp(&p->current);
}

static int query_format(struct vo *vo, uint32_t fmt)
{
    if (mp_sws_supported_format(fmt))
        return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW;
    return 0;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;

    mp_image_unrefp(&p->current);
}

static int preinit(struct vo *vo)
{
    vo->untimed = true;
    return 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    return VO_NOTIMPL;
}

#define OPT_BASE_STRUCT struct priv

const struct vo_driver video_out_image =
{
    .info = &(const vo_info_t) {
        "Write video frames to image files",
        "image",
        "wm4",
        ""
    },
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_SUBSTRUCT("", opts, image_writer_conf, 0),
        OPT_STRING("outdir", outdir, 0),
        {0},
    },
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .draw_osd = draw_osd,
    .flip_page = flip_page,
    .uninit = uninit,
};
