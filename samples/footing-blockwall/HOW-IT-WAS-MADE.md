# How this sample was made

This sample connects three authoring tasks: **build ordinary editable H5P, turn timed diagrams into video, and generate narration from an authorised voice reference**. The Opus plugin plays the finished package; it does not run the authoring tools.

## 1. Start with the teaching sequence

A five-stage footing and blockwall explainer informed the sequence. The original nine-slide lesson was written around one checking habit: inspect work while it is still accessible, before it becomes concealed. Its original diagrams deliberately omit design dimensions and use simplified cutaways. Two formative questions test the checking decision rather than memorisation of a particular project detail.

The lesson uses native **H5P Course Presentation 1.27**, with Text, Shape, Image, Audio, Video and Multiple Choice elements. Required runtime and editor libraries were included in the package. It remains editable in a compatible H5P editor; it is not a web page embedded as a single opaque activity.

## 2. Use MCP for a live, guarded editing loop

[Model Context Protocol (MCP)](https://modelcontextprotocol.io/docs/getting-started/intro) provides a way for an agent to call tools. Here, an **ACS local Lumi bridge** exposed the running editor's native draft. This was a custom local authoring integration, not a standard feature of every Lumi installation, and its adapter is not shipped in this viewer repository.

The actual loop was:

1. List editable windows and identify the correct working copy.
2. Read the live draft, its revision and stable element IDs.
3. Patch text, text colour, shape fill and percentage-based placement using that exact revision.
4. Preview the draft and capture the visible result.
5. Save explicitly, then verify the saved H5P archive.

The bridge tool names used were `lumi_windows`, `lumi_read`, `lumi_patch`, `lumi_preview`, `lumi_capture` and `lumi_save`. It also supports adding blank/themed slides and a guarded undo. A preview updates the working store; it does not itself save the `.h5p` file. Revision checks help reject stale writes, and each working document has one assigned writer.

The bridge cannot insert arbitrary new media or arbitrary elements into an existing slide. Those additions were prepared in an H5P import copy, opened in an isolated Lumi session, then refined and saved through the live tools. This boundary matters: an agent tool being able to change text does not mean it can generate or replace every asset.

## 3. Generate short Qwen voice takes before timing the visuals

[Qwen3-TTS](https://github.com/QwenLM/Qwen3-TTS) generated the narration locally, using the `Qwen3-TTS-12Hz-1.7B-Base` model and an authorised ACS reference recording. The reference stayed outside the sample distribution.

The script was divided into labelled phrases. Short takes were generated and retained at the native 24 kHz sample rate, with their text, seeds, durations and hashes recorded locally. Alternate takes were kept separately instead of overwriting the earlier candidates. Selected phrases were assembled into six narration segments: four standalone audio tracks and two video soundtracks. Speech was not speed-stretched to fit a pre-existing animation.

ASR checks and waveform/media checks helped find problems, but they are not proof of pronunciation or natural delivery. Human listening remains a separate acceptance step. A similar voice workflow should begin with a voice reference the author is authorised to use and a small test passage, before generating an entire lesson.

## 4. Render the explainers with HyperFrames

[HyperFrames](https://github.com/heygen-com/hyperframes) rendered two original HTML/SVG/GSAP compositions: footing placement and core filling. Measured narration phrase boundaries drove the visual changes, rather than placing unrelated motion behind the voice.

Each composition was linted, previewed and rendered as a silent **1280 × 720, 30 fps** MP4. FFmpeg combined the render with its narration soundtrack. Caption files were derived from the phrase timeline, with text wrapping reviewed; subdivisions within a phrase are approximate rather than word-aligned timestamps. The final videos are approximately 34.13 and 33.63 seconds long.

The H5P contains the rendered videos and local caption tracks. It does not run HyperFrames HTML during playback, so the videos remain ordinary media inside editable H5P slides. The remaining static diagrams are original lesson graphics, and the five extra sheet images come from the supplied PDF.

## 5. Add the full-page illustrations and match the theme

The original lesson had nine slides. At the author's request, the five complete PDF pages were added in their original order as slides 10–14. They were rendered as page images, preserving all the printed notes and source references without cropping.

A later live editing pass matched the original navy header (`#245b78`), teal footer (`#1f8a84`), white lettering and warm background. The containers are native editable H5P shapes. Image placement was adjusted between them while preserving page proportions. The result is the fourteen-slide **1.1.1** sample in this folder.

The original public lesson assets retain their CC BY 4.0 licence. Adding the supplied pages does not extend that licence to those pages; the combined package records its overall licence as unspecified. See [LICENSE.md](LICENSE.md).

## 6. Verify the actual saved package

The saved archive was checked for slide/media counts, required libraries, unchanged media hashes and correct page proportions. Browser automation exercised the viewer's local helper/player with external requests blocked. It tested audio, video seeking and captions, transcript popups, question feedback/retry and stopping media during navigation. The original lesson also had a Windows WebView2 test-host check.

For the final themed revision, the original nine slides were compared with the pre-edit draft, resolving renamed media paths to their content hashes. All five new layouts were rendered and captured, including a compact viewport. **Actual Opus-panel acceptance of this exact sample and human listening remain outstanding**; these were not inferred from browser tests.

## Reuse the approach

Open the sample in a compatible H5P editor and save a new working copy. Keep questions, media and text as native H5P elements. Write the narration in short phrases, generate and check authorised voice takes, measure their timings, and build visual events around those timings. Render videos before packaging, and test the saved package in the target viewer.

The tools, models and versions are recorded in [sample-manifest.json](sample-manifest.json). This folder is a runnable sample and a workflow introduction, not a portable installation of the custom MCP bridge, voice model or original production environment.
