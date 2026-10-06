# Verification scope

Recorded 6 October 2026. The exact sample hash is in [sample-manifest.json](sample-manifest.json) and [SHA256SUMS.txt](SHA256SUMS.txt).

| Check | Observed result |
| --- | --- |
| Final archive | 14 slides; four standalone audio tracks; two narrated videos; two questions; six transcript popups; five complete PDF-page images. |
| Page integrity | All five supplied page-image hashes match their source renders. All thirteen original media files retain their bytes. |
| Themed revision | The original nine slides are unchanged relative to the pre-theme draft, comparing media by hash after editor path renaming. Ten new editable shapes supply the five headers and five footers. |
| Layout | All five picture slides render with the intended colours, white text and preserved image proportions. Page bounds fit between the containers. Large and compact browser captures were recorded. |
| Offline player, illustrated 1.1.0 | All fourteen slides loaded with external requests blocked. Four audio tracks played; both videos decoded audio, loaded captions and resumed after seeking. Both questions accepted a wrong answer, showed feedback and accepted the correct answer after retry. All six transcript popups opened. Navigation stopped media. No runtime errors or external requests were recorded. |
| Final themed 1.1.1 | All five new layouts loaded through the viewer's helper/player without runtime errors or external requests. Its original lesson content and media are unchanged from the preceding checks. The layout pass did not repeat the complete listening/playback suite. |
| Windows WebView2 test host | Detailed media checks passed on the original nine-slide 1.0.0 lesson. This is earlier-version evidence, not an Opus test of the final fourteen-slide package. |

Actual Directory Opus panel acceptance of this exact sample and human listening remain pending. Browser checks, ASR checks and standalone-host tests do not substitute for those checks.

## Check in your own Opus installation

1. Select the downloaded H5P and visit all fourteen slides.
2. Play every narration track and listen for pronunciation and pacing.
3. Play both videos, turn captions on, seek and resume.
4. Exercise question feedback and retry; open the transcripts.
5. Use fullscreen to read the PDF pages; resize the panel and try keyboard focus.
6. Switch away while media is playing. Confirm it stops, then reopen the lesson.

Autoplay and learner resume are disabled by ACS H5P Viewer. Printed sheet details are project-specific. The added PDF pages are image renders, so their fine text is not searchable and is not fully transcribed in the scene alternative text.

To verify the download in PowerShell:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath '.\acs-footing-blockwall-full-pdf-1.1.1.h5p'
```

Expected SHA-256: `70ce97604b49d754af34554298572c58ba898b632e52f7485ccd8a9eb9899828`.
