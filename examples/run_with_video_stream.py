#!/usr/bin/env python3
"""
MP4 Crash-Safe Recorder - MP4 Recover Demo with Video Stream

This script demonstrates how to use the mp4_recover_demo with real video input.
It creates a test video and runs the demo with it.

Usage:
    python run_with_video_stream.py [scenario] [input_video]
    
Examples:
    python run_with_video_stream.py 1                    # Scenario 1 with synthetic frames
    python run_with_video_stream.py 1 test_input.mp4     # Scenario 1 with real video
    python run_with_video_stream.py 1 camera.mp4         # Scenario 1 with camera recording
"""

import subprocess
import sys
import os
import argparse

def create_test_video(output_file, duration=3, color="red"):
    """Create a test video using ffmpeg"""
    print(f"Creating test video: {output_file} ({duration}s, color={color})")
    
    cmd = [
        "ffmpeg",
        "-f", "lavfi",
        "-i", f"color=c={color}:s=640x480:d={duration}",
        "-f", "lavfi",
        "-i", f"sine=f=1000:d={duration}",
        "-c:v", "libx264",
        "-preset", "fast",
        "-crf", "23",
        "-c:a", "aac",
        "-b:a", "128k",
        "-y",
        output_file
    ]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            print(f"✓ Test video created: {output_file}")
            return True
        else:
            print(f"✗ Failed to create test video")
            print(result.stderr)
            return False
    except FileNotFoundError:
        print("✗ ffmpeg not found. Please install ffmpeg.")
        return False

def run_demo(scenario, input_video=None):
    """Run the mp4_recover_demo"""
    demo_exe = "build/Debug/mp4_recover_demo.exe"
    
    if not os.path.exists(demo_exe):
        print(f"✗ Demo executable not found: {demo_exe}")
        print("Please build the project first: cmake .. && cmake --build .")
        return False
    
    cmd = [demo_exe, str(scenario)]
    if input_video:
        cmd.append(input_video)
    
    print(f"\nRunning: {' '.join(cmd)}")
    print("-" * 60)
    
    try:
        result = subprocess.run(cmd)
        return result.returncode == 0
    except Exception as e:
        print(f"✗ Failed to run demo: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(
        description="MP4 Crash-Safe Recorder - MP4 Recover Demo with Video Stream"
    )
    parser.add_argument(
        "scenario",
        type=int,
        nargs="?",
        default=1,
        help="Scenario to run (1-3)"
    )
    parser.add_argument(
        "input_video",
        nargs="?",
        help="Input video file (optional, will use synthetic frames if not provided)"
    )
    parser.add_argument(
        "--create-test-video",
        action="store_true",
        help="Create a test video before running the demo"
    )
    parser.add_argument(
        "--test-video-duration",
        type=int,
        default=3,
        help="Duration of test video in seconds (default: 3)"
    )
    parser.add_argument(
        "--test-video-color",
        default="red",
        help="Color of test video (default: red)"
    )
    
    args = parser.parse_args()
    
    # Create test video if requested
    if args.create_test_video:
        test_video = "test_input.mp4"
        if not create_test_video(test_video, args.test_video_duration, args.test_video_color):
            return 1
        args.input_video = test_video
    
    # Verify input video exists if provided
    if args.input_video and not os.path.exists(args.input_video):
        print(f"✗ Input video not found: {args.input_video}")
        return 1
    
    # Run the demo
    if run_demo(args.scenario, args.input_video):
        print("\n" + "=" * 60)
        print("✓ Demo completed successfully!")
        print("=" * 60)
        
        # List generated files
        print("\nGenerated files:")
        for f in ["encoded_normal.mp4", "encoded_crash.mp4", "encoded_QVGA.mp4", "encoded_VGA.mp4"]:
            if os.path.exists(f):
                size = os.path.getsize(f)
                print(f"  - {f} ({size} bytes)")
        
        return 0
    else:
        print("\n" + "=" * 60)
        print("✗ Demo failed!")
        print("=" * 60)
        return 1

if __name__ == "__main__":
    sys.exit(main())
