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
    public partial class GameEntityView : UserControl
    {
        private Action _undoAction;
        private string _propertyName;

        public static GameEntityView Instance { get; private set; }

        public GameEntityView()
        {
            InitializeComponent();
            DataContext = null;
            Instance = this;
            DataContextChanged += (_, __) =>
            {
                if (DataContext != null)
                {
                    (DataContext as MSEntity).PropertyChanged += (s, e) => _propertyName = e.PropertyName;
                }
            };
        }

        private Action GetRenameAction()
        {
            var dc = DataContext as MSEntity;
            var selection = dc.SelectedEntities.Select(entity => (entity, entity.Name)).ToList();
            return () =>
            {
                selection.ForEach(item => item.entity.Name = item.Name);
                (DataContext as MSEntity).Refresh();
            };
        }

        private Action GetIsEnabledAction()
        {
            var dc = DataContext as MSEntity;
            var selection = dc.SelectedEntities.Select(entity => (entity, entity.IsEnabled)).ToList();
            return () =>
            {
                selection.ForEach(item => item.entity.IsEnabled = item.IsEnabled);
                (DataContext as MSEntity).Refresh();
            };
        }

        private void OnName_TextBoxGotKeyboardFocus(object sender, KeyboardFocusChangedEventArgs e)
        {
            _propertyName = string.Empty;
            _undoAction = GetRenameAction();
        }

        private void OnName_TextBoxLostKeyboardFocus(object sender, KeyboardFocusChangedEventArgs e)
        {
            if (_propertyName == nameof(MSEntity.Name) && _undoAction != null)
            {
                var redoAction = GetRenameAction();
                Project.UndoRedo.Add(new UndoRedoAction(_undoAction, redoAction, "Rename Game Entity"));
                _propertyName = null;
            }
            _undoAction = null;
        }

        private void OnIsEnabled_CheckboxClick(object sender, RoutedEventArgs e)
        {
            var undoAction = GetIsEnabledAction();
            var dc = DataContext as MSEntity;
            dc.IsEnabled = (sender as CheckBox).IsChecked == true;
            var redoAction = GetIsEnabledAction();
            Project.UndoRedo.Add(new UndoRedoAction(undoAction, redoAction,
                dc.IsEnabled == true ? "Enable Game Entity" : "Disable Game Entity"));

        }
    }
}
