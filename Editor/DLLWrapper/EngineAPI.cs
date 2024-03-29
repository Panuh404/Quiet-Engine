﻿using QEditor.EngineAPIStructs;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Text;
using QEditor.Components;

namespace QEditor.EngineAPIStructs
{
    [StructLayout(LayoutKind.Sequential)]
    class TransformComponent
    {
        public Vector3 Position;
        public Vector3 Rotation;
        public Vector3 Scale = new Vector3(1,1,1);
    }

    [StructLayout(LayoutKind.Sequential)]
    class GameEntityDescriptor
    {
        public TransformComponent Transform = new TransformComponent();
    }
}

namespace QEditor.DLLWrapper
{
    static class EngineAPI
    {
        private const string _dllName = "QEngineDLL.dll";

        [DllImport(_dllName)]
        private static extern int CreateGameEntity(GameEntityDescriptor desc);

        public static int CreateGameEntity(GameEntity entity)
        {
            GameEntityDescriptor desc = new GameEntityDescriptor();

            // Transform Component
            {
                var c = entity.GetComponent<Transform>();
                desc.Transform.Position = c.Position;
                desc.Transform.Rotation = c.Rotation;
                desc.Transform.Scale = c.Scale;
            }

            return CreateGameEntity(desc);
        }

        [DllImport(_dllName)]
        private static extern void RemoveGameEntity(int id);

        public static void RemoveGameEntity(GameEntity entity)
        {

            RemoveGameEntity(entity.EntityId);
        }
    }
}
