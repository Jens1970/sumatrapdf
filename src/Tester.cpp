/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* A driver for various tests. The idea is that instead of having a separate
   executable and related makefile additions for each test, we have one test
   driver which dispatches desired test based on cmd-line arguments.
   Currently it only does one test: mobi file parsing. */

#include "BaseUtil.h"
#include "DirIter.h"
#include "FileUtil.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "HtmlFormatter.h"
#include "HtmlPrettyPrint.h"
#include "MobiDoc.h"
#include "Mui.h"
#include "NoFreeAllocator.h"
#include "Timer.h"
#include "WinUtil.h"
#include "ZipUtil.h"

#include "DebugLog.h"

// if true, we'll save html content of a mobi ebook as well
// as pretty-printed html to MOBI_SAVE_DIR. The name will be
// ${file}.html and ${file}_pp.html
static bool gSaveHtml = false;
// if true, we'll also save images in mobi files. The name
// will be ${file}_img_${imgNo}.[jpg|png]
// gMobiSaveHtml must be true as well
static bool gSaveImages = false;
// if true, we'll do a layout of mobi files
static bool gLayout = false;
// directory to which we'll save mobi html and images
#define MOBI_SAVE_DIR L"..\\ebooks-converted"

static int Usage()
{
    printf("Tester.exe\n");
    printf("  -mobi dirOrFile : run mobi tests in a given directory or for a given file\n");
    printf("  -layout - will also layout mobi files\n");
    printf("  -save-html] - will save html content of mobi file\n");
    printf("  -save-images - will save images extracted from mobi files\n");
    printf("  -zip-create - creates a sample zip file that needs to be manually checked that it worked\n");
    return 1;
}

static void MobiSaveHtml(const WCHAR *filePathBase, MobiDoc *mb)
{
    CrashAlwaysIf(!gSaveHtml);

    ScopedMem<WCHAR> outFile(str::Join(filePathBase, L"_pp.html"));

    size_t htmlLen;
    const char *html = mb->GetBookHtmlData(htmlLen);
    size_t ppHtmlLen;
    char *ppHtml = PrettyPrintHtml(html, htmlLen, ppHtmlLen);
    file::WriteAll(outFile.Get(), ppHtml, ppHtmlLen);

    outFile.Set(str::Join(filePathBase, L".html"));
    file::WriteAll(outFile.Get(), html, htmlLen);
}

static void MobiSaveImage(const WCHAR *filePathBase, size_t imgNo, ImageData *img)
{
    // it's valid to not have image data at a given index
    if (!img || !img->data)
        return;
    const WCHAR *ext = GfxFileExtFromData((char*)img->data, img->len);
    CrashAlwaysIf(!ext);
    ScopedMem<WCHAR> fileName(str::Format(L"%s_img_%d%s", filePathBase, imgNo, ext));
    file::WriteAll(fileName.Get(), img->data, img->len);
}

static void MobiSaveImages(const WCHAR *filePathBase, MobiDoc *mb)
{
    for (size_t i = 0; i < mb->imagesCount; i++) {
        MobiSaveImage(filePathBase, i, mb->GetImage(i+1));
    }
}

// This loads and layouts a given mobi file. Used for profiling layout process.
static void MobiLayout(MobiDoc *mobiDoc)
{
    PoolAllocator textAllocator;

    HtmlFormatterArgs args;
    args.pageDx = 640;
    args.pageDy = 480;
    args.fontName = L"Tahoma";
    args.fontSize = 12;
    args.htmlStr = mobiDoc->GetBookHtmlData(args.htmlStrLen);
    args.textAllocator = &textAllocator;

    MobiFormatter mf(&args, mobiDoc);
    Vec<HtmlPage*> *pages = mf.FormatAllPages();
    DeleteVecMembers<HtmlPage*>(*pages);
    delete pages;
}

static void MobiTestFile(const WCHAR *filePath)
{
    wprintf(L"Testing file '%s'\n", filePath);
    MobiDoc *mobiDoc = MobiDoc::CreateFromFile(filePath);
    if (!mobiDoc) {
        printf(" error: failed to parse the file\n");
        return;
    }

    if (gLayout) {
        Timer t(true);
        MobiLayout(mobiDoc);
        wprintf(L"Spent %.2f ms laying out %s\n", t.GetTimeInMs(), filePath);
    }

    if (gSaveHtml || gSaveImages) {
        // Given the name of the name of source mobi file "${srcdir}/${file}.mobi"
        // construct a base name for extracted html/image files in the form
        // "${MOBI_SAVE_DIR}/${file}" i.e. change dir to MOBI_SAVE_DIR and
        // remove the file extension
        WCHAR *dir = MOBI_SAVE_DIR;
        dir::CreateAll(dir);
        ScopedMem<WCHAR> fileName(str::Dup(path::GetBaseName(filePath)));
        ScopedMem<WCHAR> filePathBase(path::Join(dir, fileName));
        WCHAR *ext = (WCHAR*)str::FindCharLast(filePathBase.Get(), '.');
        *ext = 0;

        if (gSaveHtml)
            MobiSaveHtml(filePathBase, mobiDoc);
        if (gSaveImages)
            MobiSaveImages(filePathBase, mobiDoc);
    }

    delete mobiDoc;
}

static bool IsMobiFile(const WCHAR *f)
{
    // TODO: also .prc and .pdb ?
    return str::EndsWithI(f, L".mobi") ||
           str::EndsWithI(f, L".azw") ||
           str::EndsWithI(f, L".azw1");
}

static void MobiTestDir(WCHAR *dir)
{
    wprintf(L"Testing mobi files in '%s'\n", dir);
    DirIter di;
    if (!di.Start(dir, true)) {
        wprintf(L"Error: invalid directory '%s'\n", dir);
        return;
    }

    for (;;) {
        const WCHAR *p = di.Next();
        if (NULL == p)
            break;
        if (IsMobiFile(p))
            MobiTestFile(p);
    }
}

static void MobiTest(char *dirOrFile)
{
    WCHAR *tmp = nf::str::conv::FromAnsi(dirOrFile);

    if (file::Exists(tmp) && IsMobiFile(tmp))
        MobiTestFile(tmp);
    else
        MobiTestDir(tmp);
}

// we assume this is called from main sumatradirectory, e.g. as:
// ./obj-dbg/tester.exe, so we use the known files 
void ZipCreateTest()
{
    WCHAR *zipFileName = L"tester-tmp.zip";
    file::Delete(zipFileName);
    ZipCreator *zc = new ZipCreator();
    bool ok = zc->AddFile(L"makefile.deps");
    if (!ok) {
        printf("ZipCreateTest(): failed to add makefile.deps");
        return;
    }
    ok = zc->AddFile(L"makefile.msvc");
    if (!ok) {
        printf("ZipCreateTest(): failed to add makefile.msvc");
        return;
    }
    ok = zc->SaveAs(zipFileName);
    if (!ok) {
        printf("ZipCreateTest(): SaveAs() failed");
    }
    delete zc;
}

extern "C"
int main(int argc, char **argv)
{
    nf::AllocatorMark allocatorMark;

    int i = 1;
    int left = argc - 1;

    InitAllCommonControls();
    ScopedGdiPlus gdi;
    mui::Initialize();

    char *dirOrFile = NULL;
    bool mobiTest = false;

    while (left > 0) {
        if (str::Eq(argv[i], "-mobi")) {
            ++i; --left;
            if (left < 1)
                return Usage();
            mobiTest = true;
            dirOrFile = argv[i];
            ++i; --left;
        } else if (str::Eq(argv[i], "-layout")) {
            gLayout = true;
            ++i; --left;
        } else if (str::Eq(argv[i], "-save-html")) {
            gSaveHtml = true;
            ++i; --left;
        } else if (str::Eq(argv[i], "-save-images")) {
            gSaveImages = true;
            ++i; --left;
        } else if (str::Eq(argv[i], "-zip-create")) {
            ZipCreateTest();
            ++i; --left;
        } else {
            // unknown argument
            return Usage();
        }
    }

    if (!mobiTest)
        return Usage();
    MobiTest(dirOrFile);

    mui::Destroy();
    return 0;
}
