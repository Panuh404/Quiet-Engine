using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using QEditor.Components;
using QEditor.GameProject;
using QEditor.Utilities;

namespace QEditor.Editors
{
    /// <summary>
    /// Interaction logic for ProjectLayoutView.xaml
    /// </summary>
    public partial class ProjectLayoutView : UserControl
    {
        public ProjectLayoutView()
        {
            InitializeComponent();
        }

        private void OnAddGameEntity_ButtonClick(object sender, RoutedEventArgs e)
        {
            var button = sender as Button;
            var dataContext = button.DataContext as Scene;
            dataContext.AddGameEntityCommand.Execute(new GameEntity(dataContext) { Name = "Empty Game Entity"});
        }

        private void OnGameEntities_ListBoxSelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            GameEntityView.Instance.DataContext = null;
            var ListBox = sender as ListBox;
            if (e.AddedItems.Count > 0)
            {
                GameEntityView.Instance.DataContext = (sender as ListBox).SelectedItems[0];
            }

            var newSelection = ListBox.SelectedItems.Cast<GameEntity>().ToList();
            var previousSelection = newSelection.Except(e.AddedItems.Cast<GameEntity>()).Concat(e.RemovedItems.Cast<GameEntity>()).ToList();

            Project.UndoRedo.Add(new UndoRedoAction(
                () =>
                {
                    ListBox.UnselectAll();
                    previousSelection.ForEach(x=> (ListBox.ItemContainerGenerator.ContainerFromItem(x) as ListBoxItem).IsSelected = true);
                },
                () =>
                {
                    ListBox.UnselectAll();
                    newSelection.ForEach(x => (ListBox.ItemContainerGenerator.ContainerFromItem(x) as ListBoxItem).IsSelected = true);
                },
                "Selection changed"));
        }
    }
}
