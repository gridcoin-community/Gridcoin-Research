
Option Strict Off

Friend Structure Vector

	Public X As Single
	Public Y As Single
	Public Z As Single
	Public W As Single
End Structure

Friend Module modGRCWireFrameVector

    Public Function VectorSet(ByVal X As Single, ByVal Y As Single, ByVal Z As Single) As Vector

        VectorSet.X = X
        VectorSet.Y = Y
        VectorSet.Z = Z

    End Function

    Public Function VectorSub(ByRef V1 As Vector, ByRef V2 As Vector) As Vector

        VectorSub.X = V1.X - V2.X
        VectorSub.Y = V1.Y - V2.Y
        VectorSub.Z = V1.Z - V2.Z

    End Function

    Public Function VectorAdd(ByRef V1 As Vector, ByRef V2 As Vector) As Vector

        VectorAdd.X = V1.X + V2.X
        VectorAdd.Y = V1.Y + V2.Y
        VectorAdd.Z = V1.Z + V2.Z

    End Function

    Public Function VectorDot(ByRef V1 As Vector, ByRef V2 As Vector) As Single

        Return (V1.X * V2.X) + (V1.Y * V2.Y) + (V1.Z * V2.Z)

    End Function


    Public Function VectorCross(ByRef V1 As Vector, ByRef V2 As Vector) As Vector

        VectorCross.X = (V1.Y * V2.Z) - (V1.Z * V2.Y)
        VectorCross.Y = (V1.Z * V2.X) - (V1.X * V2.Z)
        VectorCross.Z = (V1.X * V2.Y) - (V1.Y * V2.X)

    End Function

    Public Function VectorNormalize(ByRef V As Vector) As Vector

        Dim VLength As Single = Math.Sqrt((V.X ^ 2) + (V.Y ^ 2) + (V.Z ^ 2))

        If VLength = 0 Then VLength = 1
        VectorNormalize.X = V.X / VLength
        VectorNormalize.Y = V.Y / VLength
        VectorNormalize.Z = V.Z / VLength

    End Function

    Public Function VectorScale(ByRef V As Vector, ByVal S As Single) As Vector

        VectorScale.X = V.X * S
        VectorScale.Y = V.Y * S
        VectorScale.Z = V.Z * S

    End Function

    Public Sub CalculateNormal()

        Dim i As Short
        With ObjPart
            For i = 0 To .NumFaces
                .Normal(i) = VectorCross(VectorSub(.Vertices(.Faces(i).C), .Vertices(.Faces(i).B)), VectorSub(.Vertices(.Faces(i).A), .Vertices(.Faces(i).B)))
                .Normal(i) = VectorNormalize(.Normal(i))
            Next
            .NormalT = .Normal.Clone()
        End With

    End Sub

End Module
