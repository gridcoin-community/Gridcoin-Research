Imports Finisar.SQLite
Imports System.Windows.Forms
Imports System.Text

Public Class frmLeaderboard

    Private Sub frmLeaderboard_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load

        Me.Show()
        Me.Refresh()
        Me.BringToFront()


        Dim mData As New Sql("gridcoin_leaderboard")

        Dim sql As String
        'Populate Gridcoin Network Average Credits Per Project
        sql = "Select '' as Rank, avg(credits) as [Credits],factor as [Factor], avg(adjcredits) as [Adjusted Credits],ProjectName as [Project Name] from leaderboard  group by project   order by  upper(ProjectName);"


        SqlToGrid(sql, dgvNetworkStats, True, mData)
        'Populate Gridcoin Leaders:
        sql = "Select '' as Rank, avg(credits*projectcount) Credits, avg(credits*factor*projectcount) as [Adjusted Credits], avg(projectcount) as [Projects], ScryptSleepChance as [Scrypt Sleep], Address [GRC Address] from leaderboard group by Address order by avg(credits*factor*projectcount) desc "

        SqlToGrid(sql, dgvLeaders, True, mData)
        'Populate Gridcoin Leaders by Project:
        'Dim bp As BoincProject
        Dim iRow As Long = 0
        For y = 0 To UBound(modBoincLeaderboard.vProj)
            If Len(vProj(y)) > 1 Then
                Dim vProjExpanded() As String
                vProjExpanded = Split(vProj(y), "|")
                If UBound(vProjExpanded) = 1 Then
                    sql = "Select '' as Rank, avg(credits) Credits, ProjectName [Project Name], Address [GRC Address] from leaderboard " _
                        & " WHERE ProjectName='" + Trim(vProjExpanded(1)) + "'" _
                        & " Group by Address, ProjectName " _
                        & " Order by avg(credits) desc "
                    SqlToGrid(sql, dgvLeadersByProject, False, mData)
                End If
            End If
        Next
        Me.BringToFront()

    End Sub

    Private Sub SqlToGrid(sql As String, dgv As DataGridView, bClear As Boolean, mData As Sql)
        Try
            'Only used for Leaderboard Display

      
            Dim dt As DataTable
            Try
                dt = mGRCData.GetDataTable(sql)
            Catch ex As Exception
                Exit Sub
            End Try
            If dt.Rows.Count = 0 Then Exit Sub

        If bClear Or dgv.Rows.Count = 0 Then
            dgv.Rows.Clear()
            dgv.Columns.Clear()
            dgv.BackgroundColor = Drawing.Color.Black
            dgv.ForeColor = Drawing.Color.Lime
            dgv.SelectionMode = DataGridViewSelectionMode.FullRowSelect
        End If
        Dim sValue As String
            Dim iRow As Long = 0

        Try
            If dgv.Rows.Count = 0 Then
                    For x = 0 To dt.Columns.Count - 1
                        Dim dc As New System.Windows.Forms.DataGridViewColumn
                        dc.Name = dt.Columns(x).ColumnName
                        Dim dgvct As New System.Windows.Forms.DataGridViewTextBoxCell
                        dgvct.Style.BackColor = Drawing.Color.Black
                        dgvct.Style.ForeColor = Drawing.Color.Lime
                        dc.CellTemplate = dgvct
                        dgv.Columns.Add(dc)
                        Dim dcc As New DataGridViewCellStyle
                        dcc.Font = New System.Drawing.Font("Verdana bold", 10, Drawing.FontStyle.Regular)
                        dgv.Columns(x).DefaultCellStyle = dcc
                    Next x
                Dim dgcc As New DataGridViewCellStyle
                dgcc.ForeColor = System.Drawing.Color.SandyBrown
                dgv.Font = New System.Drawing.Font("Verdana", 10, Drawing.FontStyle.Bold)
                dgv.ColumnHeadersDefaultCellStyle = dgcc
                dgv.RowHeadersVisible = False
                dgv.EditMode = DataGridViewEditMode.EditProgrammatically
                    For x = 0 To dt.Columns.Count - 1
                        dgv.Columns(x).AutoSizeMode = DataGridViewAutoSizeColumnMode.AllCells
                    Next x
                dgv.ReadOnly = True

            End If
                Dim yRow As Long = 0
                For y = 0 To dt.Rows.Count - 1

                    dgv.Rows.Add()
                    iRow = iRow + 1
                    yRow += 1
                    For x = 0 To dt.Columns.Count - 1
                        sValue = ""

                        Try
                            sValue = dt.Rows(yRow)(x).ToString
                        Catch ex As Exception

                        End Try
                        If x = 0 Then sValue = iRow
                        Try

                            dgv.Rows(dgv.Rows.Count - 2).Cells(x).Value = Trim(sValue)

                        Catch ex As Exception

                        End Try

                    Next x

                Next y
                Application.DoEvents()
                Me.BringToFront()

            Exit Sub
        Catch ex As Exception
            Exit Sub
            End Try

        Catch ex As Exception

        End Try

    End Sub
    
End Class