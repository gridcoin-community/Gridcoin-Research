


Public Class Form1

    Private Sub Button1_Click(sender As System.Object, e As System.EventArgs) Handles Button1.Click
        Dim x As List(Of GPUEnumerator.GPU)
        Dim y As New GPUEnumerator.Enumeration
        x = y.Enumerate

        Stop

    End Sub
End Class
