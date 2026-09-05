/*
 * Copyright (c) 2025 Rune Skovbo Johansen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// Dither3DTextureMaker.cs - Texture generation reference
//
// This file shows how Dither3D textures are generated.
// The PNG files are 2D representations of 3D textures:
//   - Width = 16 * dotsPerSide
//   - Height = Width * layers (stacked vertically)
//   - For 8x8: 128x8192 PNG → 128x128x64 3D texture

using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

public class Dither3DTextureMaker : MonoBehaviour
{
    internal const string kTexturesPath = "Assets/Dither3D";

    [MenuItem("Assets/Create/Dither 3D Texture/Bayer 1x1")]
    static void CreateDither3DTexture1x1() { CreateDither3DTexture(0); }

    [MenuItem("Assets/Create/Dither 3D Texture/Bayer 2x2")]
    static void CreateDither3DTexture2x2() { CreateDither3DTexture(1); }

    [MenuItem("Assets/Create/Dither 3D Texture/Bayer 4x4")]
    static void CreateDither3DTexture4x4() { CreateDither3DTexture(2); }

    [MenuItem("Assets/Create/Dither 3D Texture/Bayer 8x8")]
    static void CreateDither3DTexture8x8() { CreateDither3DTexture(3); }

    static void CreateDither3DTexture(int recursion)
    {
        // Create Bayer points.
        List<Vector2> bayerPoints = new List<Vector2>();
        bayerPoints.Add(new Vector2(0.00f, 0.00f));
        bayerPoints.Add(new Vector2(0.50f, 0.50f));
        bayerPoints.Add(new Vector2(0.50f, 0.00f));
        bayerPoints.Add(new Vector2(0.00f, 0.50f));

        for (int r = 0; r < recursion - 1; r++)
        {
            int count = bayerPoints.Count;
            float offset = Mathf.Pow(0.5f, r + 1);

            for (int i = 1; i < 4; i++)
            {
                for (int j = 0; j < count; j++)
                {
                    bayerPoints.Add(bayerPoints[j] + bayerPoints[i] * offset);
                }
            }
        }

        // Determine the texture size.
        int dotsPerSide = Mathf.RoundToInt(Mathf.Pow(2, recursion));
        int layers = dotsPerSide * dotsPerSide;
        int size = 16 * dotsPerSide;

        // Configure the texture.
        Texture3D texture = new Texture3D(size, size, layers, TextureFormat.R8, false);
        texture.wrapMode = TextureWrapMode.Repeat;
        Color[] colors = new Color[size * size * layers];

        // Keep track of how many pixels are above given brightness levels,
        // so we can construct a brightness lookup curve.
        int bucketCount = 256;
        int[] brightnessBuckets = new int[bucketCount];

        // Populate the array so that the x, y, and z values of the texture will
        // map to red, blue, and green colors
        float invRes = 1.0f / size;
        for (int z = 0; z < layers; z++)
        {
            int dotCount = z + 1;
            float dotArea = 0.5f / dotCount;
            float dotRadius = Mathf.Sqrt(dotArea / Mathf.PI);

            int zOffset = z * size * size;
            for (int y = 0; y < size; y++)
            {
                int yOffset = y * size;
                for (int x = 0; x < size; x++)
                {
                    Vector2 point = new Vector2((x + 0.5f) * invRes, (y + 0.5f) * invRes);
                    float dist = Mathf.Infinity;
                    for (int i = 0; i < dotCount; i++)
                    {
                        Vector2 vec = point - bayerPoints[i];
                        vec.x = Mathf.Repeat(vec.x + 0.5f, 1) - 0.5f;
                        vec.y = Mathf.Repeat(vec.y + 0.5f, 1) - 0.5f;

                        float d = Mathf.Sqrt(vec.x * vec.x + vec.y * vec.y);
                        if (d < dist)
                            dist = d;
                    }

                    float value = dist / dotRadius;
                    value = Mathf.Clamp01(value);
                    int brightnessLevel = (int)(value * (bucketCount - 1));
                    brightnessBuckets[brightnessLevel]++;

                    colors[zOffset + yOffset + x] = new Color(value, 1 - value, value * value);
                }
            }
        }

        // Save the texture to a PNG file.
        // The PNG is a 2D representation where height = width * depth
        Texture2D pngTexture = new Texture2D(size, size * layers, TextureFormat.R8, false);
        pngTexture.SetPixels(Encode3DTo2D(colors, size, layers));
        pngTexture.Apply();

        byte[] pngData = pngTexture.EncodeToPNG();
        string path = kTexturesPath + "/Dither3D_" + dotsPerSide + "x" + dotsPerSide + ".png";
        System.IO.File.WriteAllBytes(path, pngData);
        AssetDatabase.Refresh();

        // Generate ramp texture
        int[] sortedBuckets = (int[])brightnessBuckets.Clone();
        System.Array.Sort(sortedBuckets);
        System.Array.Reverse(sortedBuckets);

        Texture2D rampTexture = new Texture2D(bucketCount, 1, TextureFormat.R8, false);
        Color[] rampColors = new Color[bucketCount];
        for (int i = 0; i < bucketCount; i++)
        {
            float t = (float)i / (bucketCount - 1);
            rampColors[i] = new Color(t, 0.5f, 1.0f - t, 1.0f);
        }
        rampTexture.SetPixels(rampColors);
        rampTexture.Apply();

        byte[] rampData = rampTexture.EncodeToPNG();
        string rampPath = kTexturesPath + "/Dither3D_" + dotsPerSide + "x" + dotsPerSide + "_Ramp.png";
        System.IO.File.WriteAllBytes(rampPath, rampData);
        AssetDatabase.Refresh();

        Debug.Log("Created " + path + " and " + rampPath);
    }

    // Encode 3D texture data to 2D layout (width x (width*depth))
    static Color[] Encode3DTo2D(Color[] data, int size, int layers)
    {
        Color[] result = new Color[size * size * layers];
        for (int z = 0; z < layers; z++)
        {
            for (int y = 0; y < size; y++)
            {
                for (int x = 0; x < size; x++)
                {
                    int srcIdx = z * size * size + y * size + x;
                    int dstIdx = y * size + x + z * size * size; // Stack layers vertically
                    result[dstIdx] = data[srcIdx];
                }
            }
        }
        return result;
    }
}
