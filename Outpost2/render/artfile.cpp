#include "artfile.h"

#include <QFile>
#include <QPainter>
#include <algorithm>
#include <climits>
#include <cstring>
#include <vector>

namespace {

// Little-endian cursor over a byte buffer.
class Cursor {
public:
    Cursor(const std::uint8_t* data, std::size_t size) : d_(data), n_(size) {}

    bool ok() const { return pos_ <= n_; }
    std::size_t pos() const { return pos_; }
    bool atEnd() const { return pos_ >= n_; }
    std::size_t remaining() const { return pos_ < n_ ? n_ - pos_ : 0; }

    std::uint8_t u8() { return (pos_ < n_) ? d_[pos_++] : (pos_++, 0); }
    std::int16_t i16() { return static_cast<std::int16_t>(u16()); }
    std::uint16_t u16() {
        std::uint16_t v = static_cast<std::uint16_t>(u8());
        v |= static_cast<std::uint16_t>(u8()) << 8;
        return v;
    }
    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }
    std::uint32_t u32() {
        std::uint32_t v = u8();
        v |= static_cast<std::uint32_t>(u8()) << 8;
        v |= static_cast<std::uint32_t>(u8()) << 16;
        v |= static_cast<std::uint32_t>(u8()) << 24;
        return v;
    }
    void tag(char out[5]) {
        for (int i = 0; i < 4; ++i) out[i] = static_cast<char>(u8());
        out[4] = '\0';
    }
    void skip(std::size_t n) { pos_ += n; }

private:
    const std::uint8_t* d_;
    std::size_t n_;
    std::size_t pos_ = 0;
};

} // namespace

bool ArtFile::load(const QString& prtPath, const QString& bmpPath, QString* error) {
    palettes_.clear();
    images_.clear();
    groups_.clear();
    frames_.clear();
    pictures_.clear();
    pixels_.clear();

    // --- read OP2_ART.BMP and keep its pixel blob -------------------------
    {
        QFile bf(bmpPath);
        if (!bf.open(QIODevice::ReadOnly)) {
            if (error) *error = "art: cannot open BMP " + bmpPath;
            return false;
        }
        const QByteArray bmp = bf.readAll();
        if (bmp.size() < 54) {
            if (error) *error = "art: BMP too small";
            return false;
        }
        const std::uint8_t* b = reinterpret_cast<const std::uint8_t*>(bmp.constData());
        // bfOffBits at offset 10.
        std::uint32_t offBits = b[10] | (b[11] << 8) | (b[12] << 16) | (std::uint32_t(b[13]) << 24);
        if (offBits >= static_cast<std::uint32_t>(bmp.size())) offBits = 54;
        pixels_.assign(b + offBits, b + bmp.size());
    }

    // --- read + parse op2_art.prt ----------------------------------------
    QFile pf(prtPath);
    if (!pf.open(QIODevice::ReadOnly)) {
        if (error) *error = "art: cannot open PRT " + prtPath;
        return false;
    }
    const QByteArray prt = pf.readAll();
    Cursor c(reinterpret_cast<const std::uint8_t*>(prt.constData()),
             static_cast<std::size_t>(prt.size()));

    // Palette section: 'CPAL' + count, then `count` palettes.
    char tag[5];
    c.tag(tag);
    if (std::strcmp(tag, "CPAL") != 0) {
        if (error) *error = "art: missing CPAL tag";
        return false;
    }
    const std::int32_t numPalettes = c.i32();
    palettes_.resize(numPalettes > 0 ? numPalettes : 0);
    for (int p = 0; p < numPalettes; ++p) {
        // Replicates the OP2 tag loop: PPAL container, 'head' sets the remaining
        // tag count, 'data' holds 256 colors (stored R,G,B,reserved).
        int tagsLeft = -2;
        do {
            --tagsLeft;
            c.tag(tag);
            const std::int32_t size = c.i32();
            if (std::strcmp(tag, "PPAL") == 0) {
                // container; no body to consume here
            } else if (std::strcmp(tag, "head") == 0) {
                tagsLeft = c.i32();
            } else if (std::strcmp(tag, "data") == 0) {
                for (int i = 0; i < 256; ++i) {
                    const std::uint8_t r = c.u8();
                    const std::uint8_t g = c.u8();
                    const std::uint8_t bb = c.u8();
                    c.u8(); // reserved
                    palettes_[p].colors[i] =
                        0xFF000000u | (std::uint32_t(r) << 16) | (std::uint32_t(g) << 8) | bb;
                }
            } else {
                c.skip(size > 0 ? static_cast<std::size_t>(size) : 0);
            }
        } while (tagsLeft != 0 && c.ok() && !c.atEnd());
    }

    // Image section: count, then `count` 20-byte image defs (4×i32 + 2×i16).
    const std::int32_t numImages = c.i32();
    images_.resize(numImages > 0 ? numImages : 0);
    for (int i = 0; i < numImages; ++i) {
        ImageDef& im = images_[i];
        im.pitch = c.i32();
        im.dataPtr = c.i32();
        im.height = c.i32();
        im.width = c.i32();
        im.type = c.i16();
        im.paletteNum = c.i16();
    }

    // Animation section: counts then per-group frames/pictures.
    const std::int32_t numGroups = c.i32();
    const std::int32_t numFrames = c.i32();
    const std::int32_t numPictures = c.i32();
    c.i32(); // numFrameOptional (total; not needed)

    groups_.reserve(numGroups > 0 ? numGroups : 0);
    frames_.reserve(numFrames > 0 ? numFrames : 0);
    pictures_.reserve(numPictures > 0 ? numPictures : 0);

    for (int g = 0; g < numGroups && c.ok() && !c.atEnd(); ++g) {
        c.i32();                            // unknown
        const std::int32_t selL = c.i32();  // selection box L,T,R,B
        const std::int32_t selT = c.i32();
        const std::int32_t selR = c.i32();
        const std::int32_t selB = c.i32();
        const std::int32_t dispX = c.i32(); // pixel displacement (sprite anchor)
        const std::int32_t dispY = c.i32();
        c.i32();                            // unknown2
        const std::int32_t gFrames = c.i32();

        Group grp;
        grp.numFrames = gFrames;
        grp.startFrame = static_cast<int>(frames_.size());
        grp.dispX = dispX; grp.dispY = dispY;
        grp.selL = selL; grp.selT = selT; grp.selR = selR; grp.selB = selB;

        for (int f = 0; f < gFrames; ++f) {
            const std::uint8_t numPics = c.u8();
            const std::uint8_t unknown = c.u8();

            Frame fr;
            // Optional block 1 = the turret mount offset (x,y) for this chassis
            // frame (see GAME_OBJECT / unit anim notes). Capture it; block 2 unknown.
            if (numPics & 0x80) {
                fr.optX = static_cast<std::int8_t>(c.u8());
                fr.optY = static_cast<std::int8_t>(c.u8());
                fr.hasOpt = true;
            }
            if (unknown & 0x80) { c.u8(); c.u8(); }  // optional block 2

            fr.numPictures = numPics & 0x7F;
            fr.delay = unknown & 0x7F; // game ticks before the next frame
            fr.startPicture = static_cast<int>(pictures_.size());

            for (int k = 0; k < (numPics & 0x7F); ++k) {
                Picture pic;
                pic.imageIndex = c.i16();
                c.u8();              // unused (0xFF)
                pic.order = c.u8();  // draw order (low under, high on top)
                pic.xOffset = c.i16();
                pic.yOffset = c.i16();
                pictures_.push_back(pic);
            }
            frames_.push_back(fr);
        }

        const std::int32_t numAppendices = c.i32();
        grp.startAppendix = static_cast<int>(appendices_.size());
        grp.numAppendices = numAppendices;
        for (int a = 0; a < numAppendices; ++a) {
            Appendix ap;
            ap.a = c.i32(); ap.b = c.i32(); ap.c = c.i32(); ap.d = c.i32();
            appendices_.push_back(ap);
        }
        groups_.push_back(grp);
    }

    return c.ok();
}

const ArtFile::ImageDef* ArtFile::image(int index) const {
    if (index < 0 || index >= static_cast<int>(images_.size())) return nullptr;
    return &images_[index];
}

const ArtFile::Picture* ArtFile::picture(int index) const {
    if (index < 0 || index >= static_cast<int>(pictures_.size())) return nullptr;
    return &pictures_[index];
}

int ArtFile::groupFrameStart(int group) const {
    if (group < 0 || group >= static_cast<int>(groups_.size())) return -1;
    return groups_[group].startFrame;
}
int ArtFile::groupFrameCount(int group) const {
    if (group < 0 || group >= static_cast<int>(groups_.size())) return 0;
    return groups_[group].numFrames;
}
int ArtFile::groupOfFrame(int frameIndex) const {
    // Which animation group owns a given absolute frame index? Groups partition the
    // frame list into contiguous [startFrame, startFrame+numFrames) ranges, so this is
    // a linear scan. Used to recover a whole animation (and its pixel displacement)
    // from a single known frame number (e.g. resource/wreckage frames). -1 if none.
    if (frameIndex < 0 || frameIndex >= static_cast<int>(frames_.size())) return -1;
    for (int g = 0; g < static_cast<int>(groups_.size()); ++g) {
        const int s = groups_[g].startFrame;
        if (frameIndex >= s && frameIndex < s + groups_[g].numFrames) return g;
    }
    return -1;
}
int ArtFile::framePictureStart(int frame) const {
    if (frame < 0 || frame >= static_cast<int>(frames_.size())) return -1;
    return frames_[frame].startPicture;
}
int ArtFile::framePictureCount(int frame) const {
    if (frame < 0 || frame >= static_cast<int>(frames_.size())) return 0;
    return frames_[frame].numPictures;
}
int ArtFile::frameDelay(int frame) const {
    if (frame < 0 || frame >= static_cast<int>(frames_.size())) return 0;
    return frames_[frame].delay;
}
bool ArtFile::frameTurretOffset(int frame, int* x, int* y) const {
    if (frame < 0 || frame >= static_cast<int>(frames_.size()) || !frames_[frame].hasOpt) {
        if (x) *x = 0; if (y) *y = 0; return false;
    }
    if (x) *x = frames_[frame].optX;
    if (y) *y = frames_[frame].optY;
    return true;
}
int ArtFile::groupAppendixStart(int group) const {
    if (group < 0 || group >= static_cast<int>(groups_.size())) return -1;
    return groups_[group].startAppendix;
}
int ArtFile::groupAppendixCount(int group) const {
    if (group < 0 || group >= static_cast<int>(groups_.size())) return 0;
    return groups_[group].numAppendices;
}
const ArtFile::Appendix* ArtFile::appendix(int index) const {
    if (index < 0 || index >= static_cast<int>(appendices_.size())) return nullptr;
    return &appendices_[index];
}
void ArtFile::groupDisplacement(int group, int* x, int* y) const {
    if (group < 0 || group >= static_cast<int>(groups_.size())) { if (x) *x = 0; if (y) *y = 0; return; }
    if (x) *x = groups_[group].dispX;
    if (y) *y = groups_[group].dispY;
}
void ArtFile::groupSelectionBox(int group, int* l, int* t, int* r, int* b) const {
    if (group < 0 || group >= static_cast<int>(groups_.size())) { if(l)*l=0; if(t)*t=0; if(r)*r=0; if(b)*b=0; return; }
    const Group& g = groups_[group];
    if (l) *l = g.selL; if (t) *t = g.selT; if (r) *r = g.selR; if (b) *b = g.selB;
}

QImage ArtFile::frameToQImage(int frameIndex, int* anchorX, int* anchorY,
                              bool includeShadows) const {
    const int start = framePictureStart(frameIndex);
    const int count = framePictureCount(frameIndex);
    if (start < 0 || count <= 0) return {};

    auto isShadow = [&](const ImageDef* im) {
        return im && (im->type == 4 || im->type == 5);
    };

    // Collect the pictures to draw, then sort by draw order so shadow pictures
    // (low order) land UNDER the building (high order). Drawing in file order is
    // what speckled the sprite before — shadows landed on top of the body.
    std::vector<int> draw;
    draw.reserve(count);
    for (int i = 0; i < count; ++i) {
        const Picture* pic = picture(start + i);
        const ImageDef* im = pic ? image(pic->imageIndex) : nullptr;
        if (!im || im->width <= 0 || im->height <= 0) continue;
        if (!includeShadows && isShadow(im)) continue;
        draw.push_back(start + i);
    }
    if (draw.empty()) return {};
    std::stable_sort(draw.begin(), draw.end(), [&](int a, int b) {
        return picture(a)->order < picture(b)->order;
    });

    // Bounding box across the chosen pictures (offsets can be negative).
    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    for (int idx : draw) {
        const Picture* pic = picture(idx);
        const ImageDef* im = image(pic->imageIndex);
        minX = std::min(minX, static_cast<int>(pic->xOffset));
        minY = std::min(minY, static_cast<int>(pic->yOffset));
        maxX = std::max(maxX, pic->xOffset + im->width);
        maxY = std::max(maxY, pic->yOffset + im->height);
    }

    QImage out(maxX - minX, maxY - minY, QImage::Format_ARGB32);
    out.fill(Qt::transparent);
    QPainter p(&out);
    for (int idx : draw) {
        const Picture* pic = picture(idx);
        const QImage img = imageToQImage(pic->imageIndex);
        if (img.isNull()) continue;
        p.drawImage(pic->xOffset - minX, pic->yOffset - minY, img);
    }
    p.end();

    if (anchorX) *anchorX = -minX;
    if (anchorY) *anchorY = -minY;
    return out;
}

QImage ArtFile::imageToQImage(int index) const {
    const ImageDef* im = image(index);
    if (!im || im->width <= 0 || im->height <= 0) return {};

    QImage out(im->width, im->height, QImage::Format_ARGB32);
    out.fill(Qt::transparent);

    const std::size_t base = static_cast<std::size_t>(im->dataPtr);
    const int w = im->width;
    const int h = im->height;

    if (im->type == 4 || im->type == 5) {
        // Shadow images are 1bpp masks (per OP2ArtReader's RenderShadow). The
        // stored pitch is the 8bpp-style pixel width and must NOT be used — 1bpp
        // rows are packed to a 32-bit boundary, MSB-first within each byte. A set
        // bit halves the destination (50% black); cleared = transparent. Getting
        // the stride wrong is what made the mask look like a dither.
        const std::size_t stride = static_cast<std::size_t>(((w + 31) / 32) * 4);
        for (int y = 0; y < h; ++y) {
            std::uint32_t* dst = reinterpret_cast<std::uint32_t*>(out.scanLine(y));
            const std::size_t ro = base + static_cast<std::size_t>(y) * stride;
            for (int x = 0; x < w; ++x) {
                const std::size_t bi = ro + static_cast<std::size_t>(x >> 3);
                const int bit = (bi < pixels_.size())
                                    ? ((pixels_[bi] >> (7 - (x & 7))) & 1) : 0;
                dst[x] = bit ? 0x80000000u : 0u; // ~50% black where the shadow is drawn
            }
        }
        return out;
    }

    const Palette* pal = (im->paletteNum >= 0 && im->paletteNum < static_cast<int>(palettes_.size()))
                             ? &palettes_[im->paletteNum]
                             : nullptr;
    for (int y = 0; y < h; ++y) {
        std::uint32_t* dst = reinterpret_cast<std::uint32_t*>(out.scanLine(y));
        const std::size_t rowOff = base + static_cast<std::size_t>(y) * im->pitch;
        for (int x = 0; x < w; ++x) {
            const std::size_t pi = rowOff + static_cast<std::size_t>(x);
            if (pi >= pixels_.size()) { dst[x] = 0u; continue; }
            const std::uint8_t idx = pixels_[pi];
            dst[x] = (idx == 0 || !pal) ? 0u : pal->colors[idx]; // 0 = transparent
        }
    }
    return out;
}
