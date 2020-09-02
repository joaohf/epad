defmodule Epad.Hubs do
  use Supervisor

  def start_link(opts) do
    Supervisor.start_link(__MODULE__, opts, name: __MODULE__)
  end

  # Callbacks

  @impl true
  def init(_opts) do
    Supervisor.init(children(), strategy: :one_for_one, max_restarts: 5, max_seconds: 60)
  end

  defp children() do
    [
      Epad.Hubs.Touchpad,
      Epad.Hubs.Keyboard,
      Epad.Hubs.Gamepad
    ]
  end
end
