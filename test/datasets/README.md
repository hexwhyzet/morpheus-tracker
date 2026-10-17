# Dataset Format Documentation

## Dataset layout

Every dataset is a self-contained directory holding its configuration and data:

```
datasets/<dataset_name>/
├── dataset.json          # Main dataset configuration
├── templates/            # Tracking templates
│   ├── t0.png
│   ├── t1.png
│   └── ...
└── sequences/            # Frame sequences
    ├── seq_000/
    │   ├── frames/       # Sequence frames
    │   │   ├── frame_0000.png
    │   │   ├── frame_0001.png
    │   │   └── ...
    │   └── gt.json       # Ground truth for this sequence
    ├── seq_001/
    │   ├── frames/
    │   └── gt.json
    └── ...
```

## dataset.json format

Describes the dataset as a whole:

```json
{
  "name": "dataset_name",
  "version": "1.0",
  "description": "Human-readable description",
  "fps": 30,
  "tags": {
    "difficulty": "easy|medium|hard",
    "environment": "indoor|outdoor",
    "custom_tag": "value"
  },
  "templates": [
    {
      "name": "t0",
      "path": "templates/t0.png",
      "metadata": {
        "width": "640",
        "height": "480"
      }
    }
  ],
  "sequences": [
    {
      "name": "seq_000",
      "type": "tracking|redetect|negative",
      "template_index": 0,
      "frames_dir": "sequences/seq_000/frames",
      "gt_path": "sequences/seq_000/gt.json",
      "metadata": {
        "description": "Sequence description",
        "num_frames": "45"
      }
    }
  ]
}
```

### dataset.json fields

- **name**: Unique dataset name
- **version**: Dataset version (for tracking changes)
- **description**: Dataset description
- **fps**: Frame rate (optional)
- **tags**: Arbitrary tags for categorization
- **templates**: Array of tracking templates
  - **name**: Template name
  - **path**: Image path (relative to dataset.json)
  - **metadata**: Arbitrary metadata
- **sequences**: Array of sequences
  - **name**: Sequence name
  - **type**: Sequence type (see below)
  - **template_index**: Index into the templates array
  - **frames_dir**: Path to the directory holding the frames
  - **gt_path**: Path to the ground truth file
  - **metadata**: Arbitrary metadata

### Sequence types

- **tracking**: Regular tracking (the object is visible in all or most frames)
- **redetect**: Redetection testing (the object disappears periodically)
- **negative**: Negative samples (the object is absent, tests for false positives)

## gt.json format

Ground truth for a single sequence:

```json
{
  "frames": [
    "frame_0000.png",
    "frame_0001.png",
    "frame_0002.png"
  ],
  "ground_truth": [
    {
      "frame_index": 0,
      "visible": true,
      "corners": [x0, y0, x1, y1, x2, y2, x3, y3],
      "occlusion": 0.0,
      "blur": 0.0,
      "noise": 0.0
    },
    {
      "frame_index": 1,
      "visible": true,
      "homography": [h00, h01, h02, h10, h11, h12, h20, h21, h22],
      "occlusion": 0.2
    },
    {
      "frame_index": 2,
      "visible": false,
      "occlusion": 1.0
    }
  ]
}
```

### gt.json fields

- **frames**: List of frame file names (in playback order)
- **ground_truth**: Per-frame ground truth array
  - **frame_index**: Frame index (matches the index in the frames array)
  - **visible**: Whether the object is visible in the frame (true/false)
  - **corners**: (Optional) Coordinates of the object's 4 corners `[x0,y0, x1,y1, x2,y2, x3,y3]`
    - Order: top-left, top-right, bottom-right, bottom-left
  - **homography**: (Optional) 3x3 homography matrix (9 elements, row-major)
  - **occlusion**: (Optional) How much of the object is occluded [0.0 - 1.0]
  - **blur**: (Optional) Amount of blur [0.0 - 1.0]
  - **noise**: (Optional) Noise level [0.0 - 1.0]

**Note**: Each frame must specify either `corners` or `homography` (when `visible=true`). Both may be given - then `corners` is used as the primary source and `homography` for extra checks.

## Usage examples

### Creating a new dataset

1. Create the directory layout
2. Put the templates into `templates/`
3. Put the frames into `sequences/seq_XXX/frames/`
4. Write `dataset.json`
5. Write `gt.json` for every sequence (an annotation script can help here)

### Loading a dataset from code

```cpp
#include "framework/dataset_format.hpp"

using namespace morpheus::test;

// Load a single dataset
DatasetConfig dataset = DatasetConfig::loadFromFile(
    "test/datasets/example_dataset/dataset.json"
);

// Resolve a template path
std::string templatePath = dataset.getTemplatePath(0);

// Load the ground truth for a sequence
SequenceGroundTruth gt = dataset.loadSequenceGroundTruth(0);

// Load every dataset in a directory
DatasetManager manager;
manager.loadDatasetsFromDir("test/datasets");

const DatasetConfig* ds = manager.getDataset("example_dataset");
```

## Conventions

### Naming

- Datasets: `<descriptive_name>` (e.g. `indoor_rotation`, `outdoor_occlusion`)
- Templates: `t0.png`, `t1.png`, ...
- Sequences: `seq_000`, `seq_001`, ...
- Frames: `frame_XXXX.png` (4-digit zero-padded numbers)

### Organization

- Group related datasets into subdirectories
- Use tags for categorization
- Document the metadata in the `description` field

### Ground Truth

- Use `corners` for simple cases
- Use `homography` for complex transformations
- Always set `visible` so the entry is interpreted correctly
- Use `occlusion`, `blur`, `noise` to analyze difficult cases

## Utilities

See `framework/dataset_format.hpp` for the dataset API:

- `DatasetConfig::loadFromFile()` - load a dataset
- `SequenceGroundTruth::loadFromFile()` - load ground truth
- `gt_utils::cornersToHomography()` - convert corners → homography
- `gt_utils::homographyToCorners()` - convert homography → corners
- `gt_utils::computeCornerError()` - compute the error between corner sets
